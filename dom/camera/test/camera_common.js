function isDefinedObj(obj) {
  return typeof(obj) !== 'undefined' && obj != null;
}

function isDefined(obj) {
  return typeof(obj) !== 'undefined';
}

/* This is a simple test suite class removing the need to
   write a lot of boilerplate for camera tests. It can
   manage the platform configurations for testing, any
   cleanup required, and common actions such as fetching
   the camera or waiting for the preview to be completed.

   To create the suite:
     var suite = new CameraTestSuite();

   To add a test case to the suite:
     suite.test('test-name', function() {
       function startAutoFocus(p) {
         return suite.camera.autoFocus();
       }

       return suite.getCamera()
         .then(startAutoFocus, suite.rejectGetCamera);
     });

   Finally, to execute the test cases:
     suite.setup()
       .then(suite.run);

   Behind the scenes, suite configured the native camera
   to use the JS hardware, setup that hardware such that
   the getCamera would succeed, got a camera control
   reference and saved it to suite.camera, and after the
   tests were finished, it reset any modified state,
   released the camera object, and concluded the mochitest
   appropriately.
*/
function CameraTestSuite() {
  SimpleTest.waitForExplicitFinish();

  this._window = window;
  this._document = document;
  this.viewfinder = document.getElementById('viewfinder');
  this._tests = [];
  this.hwType = '';

  /* Ensure that the this pointer is bound to all functions so that
     they may be used as promise resolve/reject handlers without any
     special effort, permitting code like this:

       getCamera().catch(suite.rejectGetCamera);

     instead of:

       getCamera().catch(suite.rejectGetCamera.bind(suite));
  */
  this.setup = this._setup.bind(this);
  this.teardown = this._teardown.bind(this);
  this.test = this._test.bind(this);
  this.run = this._run.bind(this);
  this.waitPreviewStarted = this._waitPreviewStarted.bind(this);
  this.waitParameterPush = this._waitParameterPush.bind(this);
  this.initJsHw = this._initJsHw.bind(this);
  this.getCamera = this._getCamera.bind(this);
  this.setLowMemoryPlatform = this._setLowMemoryPlatform.bind(this);
  this.logError = this._logError.bind(this);
  this.expectedError = this._expectedError.bind(this);
  this.expectedRejectGetCamera = this._expectedRejectGetCamera.bind(this);
  this.expectedRejectConfigure = this._expectedRejectConfigure.bind(this);
  this.expectedRejectAutoFocus = this._expectedRejectAutoFocus.bind(this);
  this.expectedRejectTakePicture = this._expectedRejectTakePicture.bind(this);
  this.expectedRejectStartRecording = this._expectedRejectStartRecording.bind(this);
  this.expectedRejectStopRecording = this._expectedRejectStopRecording.bind(this);
  this.rejectGetCamera = this._rejectGetCamera.bind(this);
  this.rejectConfigure = this._rejectConfigure.bind(this);
  this.rejectRelease = this._rejectRelease.bind(this);
  this.rejectAutoFocus = this._rejectAutoFocus.bind(this);
  this.rejectTakePicture = this._rejectTakePicture.bind(this);
  this.rejectStartRecording = this._rejectStartRecording.bind(this);
  this.rejectStopRecording = this._rejectStopRecording.bind(this);
  this.rejectPreviewStarted = this._rejectPreviewStarted.bind(this);

  var self = this;
  this._window.addEventListener('beforeunload', function() {
    if (isDefinedObj(self.viewfinder)) {
      self.viewfinder.mozSrcObject = null;
    }

    self.hw = null;
    if (isDefinedObj(self.camera)) {
      ok(false, 'window unload triggered camera release instead of test completion');
      self.camera.release();
      self.camera = null;
    }
  });
}

CameraTestSuite.prototype = {
  camera: null,
  hw: null,
  _lowMemSet: false,
  _reloading: false,

  _setupPermission: function(permission) {
    if (!SpecialPowers.hasPermission(permission, document)) {
      info("requesting " + permission + " permission");
      SpecialPowers.addPermission(permission, true, document);
      this._reloading = true;
    }
  },

  /* Returns a promise which is resolved when the test suite is ready
     to be executing individual test cases. One may provide the expected
     hardware type here if desired; the default is to use the JS test
     hardware. Use '' for the native emulated camera hardware. */
  _setup: function(hwType) {
    /* Depending on how we run the mochitest, we may not have the necessary
       permissions yet. If we do need to request them, then we have to reload
       the window to ensure the reconfiguration propogated properly. */
    this._setupPermission("camera");
    this._setupPermission("device-storage:videos");
    this._setupPermission("device-storage:videos-create");
    this._setupPermission("device-storage:videos-write");

    if (this._reloading) {
      window.location.reload();
      return Promise.reject();
    }

    info("has necessary permissions");
    if (!isDefined(hwType)) {
      hwType = 'hardware';
    }

    this._hwType = hwType;
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPrefEnv({'set': [['device.storage.prompt.testing', true]]}, function() {
        SpecialPowers.pushPrefEnv({'set': [['camera.control.test.permission', true]]}, function() {
          SpecialPowers.pushPrefEnv({'set': [['camera.control.test.enabled', hwType]]}, function() {
            resolve();
          });
        });
      });
    });
  },

  /* Returns a promise which is resolved when all of the SpecialPowers
     parameters that were set while testing are flushed. This includes
     camera.control.test.enabled and camera.control.test.is_low_memory. */
  _teardown: function() {
    return new Promise(function(resolve, reject) {
      SpecialPowers.flushPrefEnv(function() {
        resolve();
      });
    });
  },

  /* Returns a promise which is resolved when the set low memory
     parameter is set. If no value is given, it defaults to true.
     This is intended to be used inside a test case at the beginning
     of its promise chain to configure the platform as desired. */
  _setLowMemoryPlatform: function(val) {
    if (typeof(val) === 'undefined') {
      val = true;
    }

    if (this._lowMemSet === val) {
      return Promise.resolve();
    }

    var self = this;
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPrefEnv({'set': [['camera.control.test.is_low_memory', val]]}, function() {
        self._lowMemSet = val;
        resolve();
      });
    }).catch(function(e) {
      return self.logError('set low memory ' + val + ' failed', e);
    });
  },

  /* Add a test case to the test suite to be executed later. */
  _test: function(aName, aCb) {
    this._tests.push({
      name: aName,
      cb: aCb
    });
  },

  /* Execute all test cases (after setup is called). */
  _run: function() {
    if (this._reloading) {
      return;
    }

    var test = this._tests.shift();
    var self = this;
    if (test) {
      info(test.name + ' started');

      function runNextTest() {
        self.run();
      }

      function resetLowMem() {
        return self.setLowMemoryPlatform(false);
      }

      function postTest(pass) {
        ok(pass, test.name + ' finished');
        var camera = self.camera;
        self.viewfinder.mozSrcObject = null;
        self.camera = null;

        if (!isDefinedObj(camera)) {
          return Promise.resolve();
        }

        function handler(e) {
          ok(typeof(e) === 'undefined', 'camera released');
          return Promise.resolve();
        }

        return camera.release().then(handler).catch(handler);
      }

      this.initJsHw();

      var testPromise;
      try {
        testPromise = test.cb();
        if (!isDefinedObj(testPromise)) {
          testPromise = Promise.resolve();
        }
      } catch(e) {
        ok(false, 'caught exception while running test: ' + e);
        testPromise = Promise.reject(e);
      }

      testPromise
        .then(function(p) {
          return postTest(true);
        }, function(e) {
          self.logError('unhandled error', e);
          return postTest(false);
        })
        .then(resetLowMem, resetLowMem)
        .then(runNextTest, runNextTest);
    } else {
      ok(true, 'all tests completed');
      var finish = SimpleTest.finish.bind(SimpleTest);
      this.teardown().then(finish, finish);
    }
  },

  /* If the JS hardware is in use, get (and possibly initialize)
     the service XPCOM object. The native Gonk layers are able
     to get it via the same mechanism. Save a reference to it
     so that the test case may manipulate it as it sees fit in
     this.hw. Minimal setup is done for the test hardware such
     that the camera is able to be brought up without issue.

     This function has no effect if the JS hardware is not used. */
  _initJsHw: function() {
    if (this._hwType === 'hardware') {
      this.hw = SpecialPowers.Cc['@mozilla.org/cameratesthardware;1']
                .getService(SpecialPowers.Ci.nsICameraTestHardware);
      this.hw.reset(this._window);

      /* Minimum parameters required to get camera started */
      this.hw.params['preview-size'] = '320x240';
      this.hw.params['preview-size-values'] = '320x240';
      this.hw.params['picture-size-values'] = '320x240';
    } else {
      this.hw = null;
    }
  },

  /* Returns a promise which resolves when the camera has
     been successfully opened with the given name and
     configuration. If no name is given, it uses the first
     camera in the list from the camera manager. */
  _getCamera: function(name, config) {
    var cameraManager = navigator.mozCameras;
    if (!isDefined(name)) {
      name = cameraManager.getListOfCameras()[0];
    }

    var self = this;
    return cameraManager.getCamera(name, config).then(
      function(p) {
        ok(isDefinedObj(p) && isDefinedObj(p.camera), 'got camera');
        self.camera = p.camera;
        /* Ensure a followup promise can verify config by
           returning the same parameter again. */
        return Promise.resolve(p);
      }
    );
  },

  /* Returns a promise which resolves when the camera has
     successfully started the preview and is bound to the
     given viewfinder object. Note that this requires that
     a video element be present with the ID 'viewfinder'. */
  _waitPreviewStarted: function() {
    var self = this;

    return new Promise(function(resolve, reject) {
      function onPreviewStateChange(e) {
        try {
          if (e.newState === 'started') {
            ok(true, 'viewfinder is ready and playing');
            self.camera.removeEventListener('previewstatechange', onPreviewStateChange);
            resolve();
          }
        } catch(e) {
          reject(e);
        }
      }

      if (!isDefinedObj(self.viewfinder)) {
        reject(new Error('no viewfinder object'));
        return;
      }

      self.viewfinder.mozSrcObject = self.camera;
      self.viewfinder.play();
      self.camera.addEventListener('previewstatechange', onPreviewStateChange);
    });
  },

  /* Returns a promise which resolves when the camera hardware
     has received a push parameters request. This is useful
     when setting camera parameters from the application and
     you want confirmation when the operation is complete if
     there is no asynchronous notification provided. */
  _waitParameterPush: function() {
    var self = this;

    return new Promise(function(resolve, reject) {
      self.hw.attach({
        'pushParameters': function() {
          self._window.setTimeout(resolve);
        }
      });
    });
  },

  /* When an error occurs in the promise chain, all of the relevant rejection
     functions will be triggered. Most of the time however we only want the
     first rejection to be handled and then let the failure trickle down the
     chain to terminate the test. There is no way to exit a promise chain
     early so the convention is to handle the error in the first reject and
     then give an empty error for subsequent reject handlers so they know
     it is not for them.

     For example:
       function rejectSomething(e) {
         return suite.logError('something call failed');
       }

       getCamera()
         .then(, suite.rejectGetCamera)
         .then(something)
         .then(, rejectSomething)

     If the getCamera promise is rejected, suite.rejectGetCamera reports an
     error, but rejectSomething remains silent. */
  _logError: function(msg, e) {
    if (isDefined(e)) {
      ok(false, msg + ': ' + e);
    }
    // Make sure the error is undefined for later handlers
    return Promise.reject();
  },

  /* The reject handlers below are intended to be used
     when a test case does not expect a particular call
     to fail but otherwise does not require any special
     handling of that situation beyond failing the test
     case and logging why.*/
  _rejectGetCamera: function(e) {
    return this.logError('get camera failed', e);
  },

  _rejectConfigure: function(e) {
    return this.logError('set configuration failed', e);
  },

  _rejectRelease: function(e) {
    return this.logError('release camera failed', e);
  },

  _rejectAutoFocus: function(e) {
    return this.logError('auto focus failed', e);
  },

  _rejectTakePicture: function(e) {
    return this.logError('take picture failed', e);
  },

  _rejectStartRecording: function(e) {
    return this.logError('start recording failed', e);
  },

  _rejectStopRecording: function(e) {
    return this.logError('stop recording failed', e);
  },

  _rejectPreviewStarted: function(e) {
    return this.logError('preview start failed', e);
  },

  /* The success handlers below are intended to be used
     when a test case does not expect a particular call
     to succed but otherwise does not require any special
     handling of that situation beyond failing the test
     case and logging why.*/
  _expectedError: function(msg) {
    ok(false, msg);
    /* Since the original promise was technically resolved
       we actually want to pass up a rejection to try and
       end the test case sooner */
    return Promise.reject();
  },

  _expectedRejectGetCamera: function(p) {
    /* Copy handle to ensure it gets released at the end
       of the test case */
    self.camera = p.camera;
    return this.expectedError('expected get camera to fail');
  },

  _expectedRejectConfigure: function(p) {
    return this.expectedError('expected set configuration to fail');
  },

  _expectedRejectAutoFocus: function(p) {
    return this.expectedError('expected auto focus to fail');
  },

  _expectedRejectTakePicture: function(p) {
    return this.expectedError('expected take picture to fail');
  },

  _expectedRejectStartRecording: function(p) {
    return this.expectedError('expected start recording to fail');
  },

  _expectedRejectStopRecording: function(p) {
    return this.expectedError('expected stop recording to fail');
  },
};

ise(SpecialPowers.sanityCheck(), "foo", "SpecialPowers passed sanity check");
