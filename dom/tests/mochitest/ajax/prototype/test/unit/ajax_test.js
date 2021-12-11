var extendDefault = function(options) {
  return Object.extend({
    asynchronous: false,
    method: 'get',
    onException: function(request, e) { throw e }
  }, options);
};

new Test.Unit.Runner({
  setup: function() {
    $('content').update('');
    $('content2').update('');
  },
  
  teardown: function() {
    // hack to cleanup responders
    Ajax.Responders.responders = [Ajax.Responders.responders[0]];
  },
  
  testSynchronousRequest: function() {
    this.assertEqual("", $("content").innerHTML);
    
    this.assertEqual(0, Ajax.activeRequestCount);
    new Ajax.Request("../fixtures/hello.js", extendDefault({
      asynchronous: false,
      method: 'GET',
      evalJS: 'force'
    }));
    this.assertEqual(0, Ajax.activeRequestCount);
    
    var h2 = $("content").firstChild;
    this.assertEqual("Hello world!", h2.innerHTML);
  },
  
  testUpdaterOptions: function() {
    var options = {
      method: 'get',
      asynchronous: false,
      evalJS: 'force',
      onComplete: Prototype.emptyFunction
    }
    var request = new Ajax.Updater("content", "../fixtures/hello.js", options);
    request.options.onComplete = function() {};
    this.assertIdentical(Prototype.emptyFunction, options.onComplete);
  },
  
  testEvalResponseShouldBeCalledBeforeOnComplete: function() {
    if (this.isRunningFromRake) {
      this.assertEqual("", $("content").innerHTML);
    
      this.assertEqual(0, Ajax.activeRequestCount);
      new Ajax.Request("../fixtures/hello.js", extendDefault({
        onComplete: function(response) { this.assertNotEqual("", $("content").innerHTML) }.bind(this)
      }));
      this.assertEqual(0, Ajax.activeRequestCount);
    
      var h2 = $("content").firstChild;
      this.assertEqual("Hello world!", h2.innerHTML);
    } else {
      this.info(message);
    }
  },
  
  testContentTypeSetForSimulatedVerbs: function() {
    if (this.isRunningFromRake) {
      new Ajax.Request('/inspect', extendDefault({
        method: 'put',
        contentType: 'application/bogus',
        onComplete: function(response) {
          this.assertEqual('application/bogus; charset=UTF-8', response.responseJSON.headers['content-type']);
        }.bind(this)
      }));
    } else {
      this.info(message);
    }
  },
  
  testOnCreateCallback: function() {
    new Ajax.Request("../fixtures/content.html", extendDefault({
      onCreate: function(transport) { this.assertEqual(0, transport.readyState) }.bind(this),
      onComplete: function(transport) { this.assertNotEqual(0, transport.readyState) }.bind(this)
    }));
  },
  
  testEvalJS: function() {
    if (this.isRunningFromRake) {
      
      $('content').update();
      new Ajax.Request("/response", extendDefault({
        parameters: Fixtures.js,
        onComplete: function(transport) { 
          var h2 = $("content").firstChild;
          this.assertEqual("Hello world!", h2.innerHTML);
        }.bind(this)
      }));
      
      $('content').update();
      new Ajax.Request("/response", extendDefault({
        evalJS: false,
        parameters: Fixtures.js,
        onComplete: function(transport) { 
          this.assertEqual("", $("content").innerHTML);
        }.bind(this)
      }));
    } else {
      this.info(message);
    }
    
    $('content').update();
    new Ajax.Request("../fixtures/hello.js", extendDefault({
      evalJS: 'force',
      onComplete: function(transport) { 
        var h2 = $("content").firstChild;
        this.assertEqual("Hello world!", h2.innerHTML);
      }.bind(this)
    }));
  },

  testCallbacks: function() {
    var options = extendDefault({
      onCreate: function(transport) { this.assertInstanceOf(Ajax.Response, transport) }.bind(this)
    });
    
    Ajax.Request.Events.each(function(state){
      options['on' + state] = options.onCreate;
    });

    new Ajax.Request("../fixtures/content.html", options);
  },

  testResponseText: function() {
    new Ajax.Request("../fixtures/empty.html", extendDefault({
      onComplete: function(transport) { this.assertEqual('', transport.responseText) }.bind(this)
    }));
    
    new Ajax.Request("../fixtures/content.html", extendDefault({
      onComplete: function(transport) { this.assertEqual(sentence, transport.responseText.toLowerCase()) }.bind(this)
    }));
  },
  
  testResponseXML: function() {
    if (this.isRunningFromRake) {
      new Ajax.Request("/response", extendDefault({
        parameters: Fixtures.xml,
        onComplete: function(transport) { 
          this.assertEqual('foo', transport.responseXML.getElementsByTagName('name')[0].getAttribute('attr'))
        }.bind(this)
      }));
    } else {
      this.info(message);
    }
  },
      
  testResponseJSON: function() {
    if (this.isRunningFromRake) {
      new Ajax.Request("/response", extendDefault({
        parameters: Fixtures.json,
        onComplete: function(transport) { this.assertEqual(123, transport.responseJSON.test) }.bind(this)
      }));
      
      new Ajax.Request("/response", extendDefault({
        parameters: {
          'Content-Length': 0,
          'Content-Type': 'application/json'
        },
        onComplete: function(transport) { this.assertNull(transport.responseJSON) }.bind(this)
      }));
      
      new Ajax.Request("/response", extendDefault({
        evalJSON: false,
        parameters: Fixtures.json,
        onComplete: function(transport) { this.assertNull(transport.responseJSON) }.bind(this)
      }));
    
      new Ajax.Request("/response", extendDefault({
        parameters: Fixtures.jsonWithoutContentType,
        onComplete: function(transport) { this.assertNull(transport.responseJSON) }.bind(this)
      }));
    
      new Ajax.Request("/response", extendDefault({
        sanitizeJSON: true,
        parameters: Fixtures.invalidJson,
        onException: function(request, error) {
          this.assert(error.message.include('Badly formed JSON string'));
          this.assertInstanceOf(Ajax.Request, request);
        }.bind(this)
      }));
    } else {
      this.info(message);
    }
    
    new Ajax.Request("../fixtures/data.json", extendDefault({
      evalJSON: 'force',
      onComplete: function(transport) { this.assertEqual(123, transport.responseJSON.test) }.bind(this)
    }));
  },
  
  testHeaderJSON: function() {
    if (this.isRunningFromRake) {
      new Ajax.Request("/response", extendDefault({
        parameters: Fixtures.headerJson,
        onComplete: function(transport, json) {
          this.assertEqual('hello #éà', transport.headerJSON.test);
          this.assertEqual('hello #éà', json.test);
        }.bind(this)
      }));
    
      new Ajax.Request("/response", extendDefault({
        onComplete: function(transport, json) { 
          this.assertNull(transport.headerJSON)
          this.assertNull(json)
        }.bind(this)
      }));
    } else {
      this.info(message);
    }
  },
  
  testGetHeader: function() {
    if (this.isRunningFromRake) {
     new Ajax.Request("/response", extendDefault({
        parameters: { 'X-TEST': 'some value' },
        onComplete: function(transport) {
          this.assertEqual('some value', transport.getHeader('X-Test'));
          this.assertNull(transport.getHeader('X-Inexistant'));
        }.bind(this)
      }));
    } else {
      this.info(message);
    }
  },
  
  testParametersCanBeHash: function() {
    if (this.isRunningFromRake) {
      new Ajax.Request("/response", extendDefault({
        parameters: $H({ "one": "two", "three": "four" }),
        onComplete: function(transport) {
          this.assertEqual("two", transport.getHeader("one"));
          this.assertEqual("four", transport.getHeader("three"));
          this.assertNull(transport.getHeader("toObject"));
        }.bind(this)
      }));
    } else {
      this.info(message);
    }
  },
  
  testIsSameOriginMethod: function() {
    var isSameOrigin = Ajax.Request.prototype.isSameOrigin;
    this.assert(isSameOrigin.call({ url: '/foo/bar.html' }), '/foo/bar.html');
    this.assert(isSameOrigin.call({ url: window.location.toString() }), window.location);
    this.assert(!isSameOrigin.call({ url: 'http://example.com' }), 'http://example.com');

    if (this.isRunningFromRake) {
      Ajax.Request.prototype.isSameOrigin = function() {
        return false
      };

      $("content").update('same origin policy');
      new Ajax.Request("/response", extendDefault({
        parameters: Fixtures.js,
        onComplete: function(transport) { 
          this.assertEqual("same origin policy", $("content").innerHTML);
        }.bind(this)
      }));

      new Ajax.Request("/response", extendDefault({
        parameters: Fixtures.invalidJson,
        onException: function(request, error) {
          this.assert(error.message.include('Badly formed JSON string'));
        }.bind(this)
      }));

      new Ajax.Request("/response", extendDefault({
        parameters: { 'X-JSON': '{});window.attacked = true;({}' },
        onException: function(request, error) {
          this.assert(error.message.include('Badly formed JSON string'));
        }.bind(this)
      }));

      Ajax.Request.prototype.isSameOrigin = isSameOrigin;
    } else {
      this.info(message);
    }
  }
});
