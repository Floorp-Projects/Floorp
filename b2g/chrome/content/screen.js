// screen.js:
// Set the screen size, pixel density and scaling of the b2g client screen
// based on the --screen command-line option, if there is one.
// 
// TODO: support multiple device pixels per CSS pixel
// 

var browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
var isMulet = "ResponsiveUI" in browserWindow;
Cu.import("resource://gre/modules/GlobalSimulatorScreen.jsm");

window.addEventListener('ContentStart', onStart);
window.addEventListener('SafeModeStart', onStart);

// We do this on ContentStart and SafeModeStart because querying the
// displayDPI fails otherwise.
function onStart() {
  // This is the toplevel <window> element
  let shell = document.getElementById('shell');

  // The <browser> element inside it
  let browser = document.getElementById('systemapp');

  // Figure out the native resolution of the screen
  let windowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Components.interfaces.nsIDOMWindowUtils);
  let hostDPI = windowUtils.displayDPI;

  let DEFAULT_SCREEN = '320x480';

  // This is a somewhat random selection of named screens.
  // Add more to this list when we support more hardware.
  // Data from: http://en.wikipedia.org/wiki/List_of_displays_by_pixel_density
  let screens = {
    iphone: {
      name: 'Apple iPhone', width:320, height:480,  dpi:163
    },
    ipad: {
      name: 'Apple iPad', width:1024, height:768,  dpi:132
    },
    nexus_s: {
      name: 'Samsung Nexus S', width:480, height:800, dpi:235
    },
    galaxy_s2: {
      name: 'Samsung Galaxy SII (I9100)', width:480, height:800, dpi:219
    },
    galaxy_nexus: {
      name: 'Samsung Galaxy Nexus', width:720, height:1280, dpi:316
    },
    galaxy_tab: {
      name: 'Samsung Galaxy Tab 10.1', width:800, height:1280, dpi:149
    },
    wildfire: {
      name: 'HTC Wildfire', width:240, height:320, dpi:125
    },
    tattoo: {
      name: 'HTC Tattoo', width:240, height:320, dpi:143
    },
    salsa: {
      name: 'HTC Salsa', width:320, height:480, dpi:170
    },
    chacha: {
      name: 'HTC ChaCha', width:320, height:480, dpi:222
    },
  };

  // Get the command line arguments that were passed to the b2g client
  let args;
  try {
    let service = Cc["@mozilla.org/commandlinehandler/general-startup;1?type=b2gcmds"].getService(Ci.nsISupports);
    args = service.wrappedJSObject.cmdLine;
  } catch(e) {}

  let screenarg = null;

  // Get the --screen argument from the command line
  try {
    if (args) {
      screenarg = args.handleFlagWithParam('screen', false);
    }

    // Override default screen size with a pref
    if (screenarg === null && Services.prefs.prefHasUserValue('b2g.screen.size')) {
      screenarg = Services.prefs.getCharPref('b2g.screen.size');
    }

    // If there isn't one, use the default screen
    if (screenarg === null)
      screenarg = DEFAULT_SCREEN;

    // With no value, tell the user how to use it
    if (screenarg == '')
      usage();
  }
  catch(e) {
    // If getting the argument value fails, its an error
    usage();
  }
  
  // Special case --screen=full goes into fullscreen mode
  if (screenarg === 'full') {
    shell.setAttribute('sizemode', 'fullscreen');
    return;
  } 

  let width, height, ratio = 1.0;
  let lastResizedWidth;

  if (screenarg in screens) {
    // If this is a named screen, get its data
    let screen = screens[screenarg];
    width = screen.width;
    height = screen.height;
    ratio = screen.ratio;
  } else {
    // Otherwise, parse the resolution and density from the --screen value.
    // The supported syntax is WIDTHxHEIGHT[@DPI]
    let match = screenarg.match(/^(\d+)x(\d+)(@(\d+(\.\d+)?))?$/);
    
    // Display usage information on syntax errors
    if (match == null)
      usage();
    
    // Convert strings to integers
    width = parseInt(match[1], 10);
    height = parseInt(match[2], 10);
    if (match[4])
      ratio = parseFloat(match[4], 10);

    // If any of the values came out 0 or NaN or undefined, display usage
    if (!width || !height || !ratio) {
      usage();
    }
  }

  Services.prefs.setCharPref('layout.css.devPixelsPerPx',
                             ratio == 1 ? -1 : ratio);
  let defaultOrientation = width < height ? 'portrait' : 'landscape';
  GlobalSimulatorScreen.mozOrientation = GlobalSimulatorScreen.screenOrientation = defaultOrientation;

  function resize() {
    GlobalSimulatorScreen.width = width;
    GlobalSimulatorScreen.height = height;

    // Set the window width and height to desired size plus chrome
    // Include the size of the toolbox displayed under the system app
    let controls = document.getElementById('controls');
    let controlsHeight = controls ? controls.getBoundingClientRect().height : 0;

    if (isMulet) {
      let tab = browserWindow.gBrowser.selectedTab;
      let responsive = ResponsiveUIManager.getResponsiveUIForTab(tab);
      responsive.setSize(width + 16*2,
                         height + controlsHeight + 61);
    } else {
      let chromewidth = window.outerWidth - window.innerWidth;
      let chromeheight = window.outerHeight - window.innerHeight + controlsHeight;

      if (lastResizedWidth == width) {
        return;
      }
      lastResizedWidth = width;

      window.resizeTo(width + chromewidth,
                      height + chromeheight);
    }

    let frameWidth = width, frameHeight = height;

    // If the current app doesn't supports the current screen orientation
    // still resize the window, but rotate its frame so that
    // it is displayed rotated on the side
    let shouldFlip = GlobalSimulatorScreen.mozOrientation != GlobalSimulatorScreen.screenOrientation;

    if (shouldFlip) {
      frameWidth = height;
      frameHeight = width;
    }

    // Set the browser element to the full unscaled size of the screen
    let style = browser.style;
    style.transform = '';
    style.height = 'calc(100% - ' + controlsHeight + 'px)';
    style.bottom = controlsHeight;

    style.width = frameWidth + "px";
    style.height = frameHeight + "px";

    if (shouldFlip) {
      // Display the system app with a 90° clockwise rotation
      let shift = Math.floor(Math.abs(frameWidth - frameHeight) / 2);
      style.transform +=
        ' rotate(0.25turn) translate(-' + shift + 'px, -' + shift + 'px)';
    }
  }

  // Resize on startup
  resize();

  // Catch manual resizes to update the internal device size.
  window.onresize = function() {
    let controls = document.getElementById('controls');
    let controlsHeight = controls ? controls.getBoundingClientRect().height : 0;

    width = window.innerWidth;
    height = window.innerHeight - controlsHeight;

    queueResize();
  };

  // Then resize on each rotation button click,
  // or when the system app lock/unlock the orientation
  Services.obs.addObserver(function orientationChangeListener(subject) {
    let screen = subject.wrappedJSObject;
    let { mozOrientation, screenOrientation } = screen;

    // If we have an orientation different than the current one,
    // we switch the sizes
    if (screenOrientation != defaultOrientation) {
      let w = width;
      width = height;
      height = w;
    }
    defaultOrientation = screenOrientation;

    queueResize();
  }, 'simulator-adjust-window-size', false);

  // Queue resize request in order to prevent race and slowdowns
  // by requesting resize multiple times per loop
  let resizeTimeout;
  function queueResize() {
    if (resizeTimeout) {
      clearTimeout(resizeTimeout);
    }
    resizeTimeout = setTimeout(function () {
      resizeTimeout = null;
      resize();
    }, 0);
  }

  // A utility function like console.log() for printing to the terminal window
  // Uses dump(), but enables it first, if necessary
  function print() {
    let dump_enabled =
      Services.prefs.getBoolPref('browser.dom.window.dump.enabled');

    if (!dump_enabled)
      Services.prefs.setBoolPref('browser.dom.window.dump.enabled', true);

    dump(Array.prototype.join.call(arguments, ' ') + '\n');

    if (!dump_enabled) 
      Services.prefs.setBoolPref('browser.dom.window.dump.enabled', false);
  }

  // Print usage info for --screen and exit
  function usage() {
    // Documentation for the --screen argument
    let msg = 
      'The --screen argument specifies the desired resolution and\n' +
      'pixel density of the simulated device screen. Use it like this:\n' +
      '\t--screen=WIDTHxHEIGHT\t\t\t// E.g.: --screen=320x480\n' +
      '\t--screen=WIDTHxHEIGHT@DOTS_PER_INCH\t// E.g.: --screen=480x800@250\n' +
      '\t--screen=full\t\t\t\t// run in fullscreen mode\n' +
      '\nYou can also specify certain device names:\n';
    for(let p in screens)
      msg += '\t--screen=' + p + '\t// ' + screens[p].name + '\n';

    // Display the usage message
    print(msg);

    // Exit the b2g client
    Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
  }
}
