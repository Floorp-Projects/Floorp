====================
Web Console remoting
====================

Introduction
************

This document describes the way Web Console remoting works. The Web Console is split between a client with its user interface, and the server which has listeners for all the things that happen in the tab. For communication between the server and the client we use the `Remote Debugging Protocol <https://wiki.mozilla.org/Remote_Debugging_Protocol>`_. This architecture allows you to connect a Web Console client instance to a server running on B2G, Fennec or some other Firefox instance.

To better understand the architecture of the Web Console we recommend learning about the `debugger architecture <https://wiki.mozilla.org/Debugger_Architecture>`_.

.. note::
  The remote Web Console is a feature introduced in Firefox 18. This document describes the latest protocol, with changes that have been made since then.


The ``WebConsoleActor`` and the ``WebConsoleClient``
****************************************************

The ``WebConsoleActor`` lives in `dbg-webconsole-actors.js <http://mxr.mozilla.org/mozilla-central/source/toolkit/devtools/webconsole/dbg-webconsole-actors.js>`_, in the `toolkit/devtools/webconsole <http://mxr.mozilla.org/mozilla-central/source/toolkit/devtools/webconsole/>`_ folder.

The ``WebConsoleClient`` lives in `WebConsoleClient.jsm <http://mxr.mozilla.org/mozilla-central/source/toolkit/devtools/webconsole/WebConsoleClient.jsm/>`_ (in `toolkit/devtools/webconsole <http://mxr.mozilla.org/mozilla-central/source/toolkit/devtools/webconsole/>`_) and it is used by the Web Console when working with the Web Console actor.

To see how the debugger is used in the Web Console code, look in `browser/devtools/webconsole/webconsole.js <http://mxr.mozilla.org/mozilla-central/source/browser/devtools/webconsole/webconsole.js/>`_, and search for ``WebConsoleConnectionProxy``.

The new Web Console actors are:

- The ``WebConsoleActor`` allows JS evaluation, autocomplete, start/stop listeners, etc.
- The ``NetworkEventActor`` is used for each new network request. The client can request further network event details - like response body or request headers.


To attach to the ``WebConsoleActor``, follow these steps:

.. code-block:: javascript

  connectToServer() // the usual
  listTabs()
  pickTheTabYouWant()
  debuggerClient.attachConsole(tab.consoleActor, listeners, onAttachConsole)


The ``listeners`` argument is an array which specifies listeners you want to start in the web console. These can be: page errors, ``window.console`` API messages, network activity, and file activity. For example:

.. code-block:: javascript

  ["PageError", "ConsoleAPI", "NetworkActivity", "FileActivity"]


.. note::
  The Web Console actor does not start any listeners by default. The client has the option to start each listener when needed. This approach allows for lower resource usage on the server - this is a potential issue if the server runs on devices with fewer resources.


The ``onAttachConsole`` callback receives a new instance of the ``WebConsoleClient`` object. This object provides methods that abstract away protocol packets, things like ``startListeners(), stopListeners()``, etc.

Protocol packets look as follows:

.. code-block:: javascript

  {
    "to": "root",
    "type": "listTabs"
  }
  {
    "from": "root",
    "consoleActor": "conn0.console9",
    "selected": 2,
    "tabs": [
      {
        "actor": "conn0.tab2",
        "consoleActor": "conn0.console7",
        "title": "",
        "url": "https://tbpl.mozilla.org/?tree=Fx-Team"
      },
  // ...
    ]
  }


Notice that the ``consoleActor`` is also available as a **global actor**. When you attach to the global ``consoleActor`` you receive all of the network requests, page errors, and the other events from all of the tabs and windows, including chrome errors and network events. This actor is used for the Browser Console implementation and for debugging remote Firefox/B2G instances.


``startListeners(listeners, onResponse)``
-----------------------------------------

The new ``startListeners`` packet:

.. code-block:: javascript

  {
    "to": "conn0.console9",
    "type": "startListeners",
    "listeners": [
      "PageError",
      "ConsoleAPI",
      "NetworkActivity",
      "FileActivity"
    ]
  }

The reply is:

.. code-block:: javascript

  {
    "startedListeners": [
      "PageError",
      "ConsoleAPI",
      "NetworkActivity",
      "FileActivity"
    ],
    "from": "conn0.console9"
  }


The reply tells which listeners were started.


Tab navigation
--------------

To listen to the tab navigation events you also need to attach to the tab actor for the given tab. The ``tabNavigated`` notification comes from tab actors.

.. warning::
  Prior to Firefox 20 the Web Console actor provided a ``LocationChange`` listener, with an associated ``locationChanged`` notification. This is no longer the case: we have made changes to allow the Web Console client to reuse the ``tabNavigated`` notification (`bug 792062 <https://bugzilla.mozilla.org/show_bug.cgi?id=792062>`_).


When page navigation starts the following packet is sent from the tab actor:

.. code-block::

  {
    "from": tabActor,
    "type": "tabNavigated",
    "state": "start",
    "url": newURL,
  }


When navigation stops the following packet is sent:

.. code-block::

  {
    "from": tabActor,
    "type": "tabNavigated",
    "state": "stop",
    "url": newURL,
    "title": newTitle,
  }


``getCachedMessages(types, onResponse)``
----------------------------------------

The ``webConsoleClient.getCachedMessages(types, onResponse)`` method sends the following packet to the server:

.. code-block:: json

  {
    "to": "conn0.console9",
    "type": "getCachedMessages",
    "messageTypes": [
      "PageError",
      "ConsoleAPI"
    ]
  }


The ``getCachedMessages`` packet allows one to retrieve the cached messages from before the Web Console was open. You can only get cached messages for page errors and console API calls. The reply looks like this:

.. code-block::

  {
    "messages": [ ... ],
    "from": "conn0.console9"
  }

Each message in the array is of the same type as when we send typical page errors and console API calls. These will be explained in the following sections of this document.


Actor preferences
-----------------

To allow the Web Console to configure logging options while it is running, we have added the ``setPreferences`` packet:

.. code-block:: json

  {
    "to": "conn0.console9",
    "type": "setPreferences",
    "preferences": {
      "NetworkMonitor.saveRequestAndResponseBodies": false
    }
  }


Reply:

.. code-block:: json

  {
    "updated": [
      "NetworkMonitor.saveRequestAndResponseBodies"
    ],
    "from": "conn0.console10"
  }

For convenience you can use ``webConsoleClient.setPreferences(prefs, onResponse)``. The ``prefs`` argument is an object like ``{ prefName: prefValue, ... }``.

In Firefox 25 we added the ``getPreferences`` request packet:

.. code-block:: json

  {
    "to": "conn0.console34",
    "type": "getPreferences",
    "preferences": [
      "NetworkMonitor.saveRequestAndResponseBodies"
    ]
  }


Reply packet:

.. code-block:: json

  {
    "preferences": {
      "NetworkMonitor.saveRequestAndResponseBodies": false
    },
    "from": "conn0.console34"
  }


You can also use the ``webConsoleClient.getPreferences(prefs, onResponse)``. The ``prefs`` argument is an array of preferences you want to get their values for, by name.


Private browsing
----------------

The Browser Console can be used while the user has private windows open. Each page error, console API message and network request is annotated with a ``private`` flag. Private messages are cleared whenever the last private window is closed. The console actor provides the ``lastPrivateContextExited`` notification:

.. code-block:: json

  {
    "from": "conn0.console19",
    "type": "lastPrivateContextExited"
  }


This notification is sent only when your client is attached to the global console actor, it does not make sense for tab console actors.

.. note::
  This notification has been introduced in Firefox 24.


Send HTTP requests
------------------

Starting with Firefox 25 you can send an HTTP request using the console actor:

.. code-block:: javascript

  {
    "to": "conn0.console9",
    "type": "sendHTTPRequest",
    "request": {
      "url": "http://localhost",
      "method": "GET",
      "headers": [
        {
          name: "Header-name",
          value: "header value",
        },
        // ...
      ],
    },
  }


The response packet is a network event actor grip:

.. code-block:: json

  {
    "to": "conn0.console9",
    "eventActor": {
      "actor": "conn0.netEvent14",
      "startedDateTime": "2013-08-26T19:50:03.699Z",
      "url": "http://localhost",
      "method": "GET"
      "isXHR": true,
      "private": false
    }
  }


You can also use the ``webConsoleClient.sendHTTPRequest(request, onResponse)`` method. The ``request`` argument is the same as the ``request`` object in the above example request packet.

Page errors
***********

Page errors come from the ``nsIConsoleService``. Each allowed page error is an ``nsIScriptError`` object.

The ``pageError`` packet is:

.. code-block:: json

  {
    "from": "conn0.console9",
    "type": "pageError",
    "pageError": {
      "errorMessage": "ReferenceError: foo is not defined",
      "sourceName": "http://localhost/~mihai/mozilla/test.js",
      "lineText": "",
      "lineNumber": 6,
      "columnNumber": 0,
      "category": "content javascript",
      "timeStamp": 1347294508210,
      "error": false,
      "warning": false,
      "exception": true,
      "strict": false,
      "private": false,
    }
  }


The packet is similar to ``nsIScriptError`` - for simplicity. We only removed several unneeded properties and changed how flags work.

The ``private`` flag tells if the error comes from a private window/tab (added in Firefox 24).

Starting with Firefox 24 the ``errorMessage`` and ``lineText`` properties can be long string actor grips if the string is very long.


Console API messages
********************

The `window.console API <https://developer.mozilla.org/en-US/docs/Web/API/console>`_ calls send internal messages throughout Gecko which allow us to do whatever we want for each call. The Web Console actor sends these messages to the remote debugging client.

We use the ``ObjectActor`` from `dbg-script-actors.js <https://mxr.mozilla.org/mozilla-central/source/toolkit/devtools/debugger/server/dbg-script-actors.js>`_ without a ``ThreadActor``, to avoid slowing down the page scripts - the debugger deoptimizes JavaScript execution in the target page. The `lifetime of object actors <https://wiki.mozilla.org/Remote_Debugging_Protocol#Grip_Lifetimes>`_ in the Web Console is different than the lifetime of these objects in the debugger - which is usually per pause or per thread. The Web Console manages the lifetime of ``ObjectActors`` manually.


.. warning::
  Prior to Firefox 23 we used a different actor (``WebConsoleObjectActor``) for working with JavaScript objects through the protocol. In `bug 783499 <https://bugzilla.mozilla.org/show_bug.cgi?id=783499>`_ we did a number of changes that allowed us to reuse the ``ObjectActor`` from the debugger.


Console API messages come through the ``nsIObserverService`` - the console object implementation lives in `dom/base/ConsoleAPI.js <http://mxr.mozilla.org/mozilla-central/source/dom/base/ConsoleAPI.js>`_.

For each console message we receive in the server, we send the following ``consoleAPICall`` packet to the client:

.. code-block:: json

  {
    "from": "conn0.console9",
    "type": "consoleAPICall",
    "message": {
      "level": "error",
      "filename": "http://localhost/~mihai/mozilla/test.html",
      "lineNumber": 149,
      "functionName": "",
      "timeStamp": 1347302713771,
      "private": false,
      "arguments": [
        "error omg aloha ",
        {
          "type": "object",
          "className": "HTMLBodyElement",
          "actor": "conn0.consoleObj20"
        },
        " 960 739 3.141592653589793 %a",
        "zuzu",
        { "type": "null" },
        { "type": "undefined" }
      ]
    }
  }

Similar to how we send the page errors, here we send the actual console event received from the ``nsIObserverService``. We change the ``arguments`` array - we create ``ObjectActor`` instances for each object passed as an argument - and, lastly, we remove some unneeded properties (like window IDs). In the case of long strings we use the ``LongStringActor``. The Web Console can then inspect the arguments.

The ``private`` flag tells if the console API call comes from a private window/tab (added in Firefox 24).

We have small variations for the object, depending on the console API call method - just like there are small differences in the console event object received from the observer service. To see these differences please look in the Console API implementation: `dom/base/ConsoleAPI.js <http://mxr.mozilla.org/mozilla-central/source/dom/base/ConsoleAPI.js>`_.


JavaScript evaluation
---------------------

The ``evaluateJS`` request and response packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Web Console client provides the ``evaluateJS(requestId, string, onResponse)`` method which sends the following packet:

.. code-block:: json

  {
    "to": "conn0.console9",
    "type": "evaluateJS",
    "text": "document",
    "bindObjectActor": null,
    "frameActor": null,
    "url": null,
    "selectedNodeActor": null,
  }


The ``bindObjectActor`` property is an optional ``ObjectActor`` ID that points to a ``Debugger.Object``. This option allows you to bind ``_self`` to the ``Debugger.Object`` of the given object actor, during string evaluation. See ``evalInGlobalWithBindings()`` for information about bindings.

.. note::
  The variable view needs to update objects and it does so by binding ``_self`` to the ``Debugger.Object`` of the ``ObjectActor`` that is being viewed. As such, variable view sends strings like these for evaluation:

.. code-block:: javascript

  _self["prop"] = value;

The ``frameActor`` property is an optional ``FrameActor`` ID. The FA holds a reference to a ``Debugger.Frame``. This option allows you to evaluate the string in the frame of the given FA.

The ``url`` property is an optional URL to evaluate the script as (new in Firefox 25). The default source URL for evaluation is "debugger eval code".

The ``selectedNodeActor`` property is an optional ``NodeActor`` ID, which is used to indicate which node is currently selected in the Inspector, if any. This ``NodeActor`` can then be referred to by the ``$0`` JSTerm helper.

The response packet:

.. code-block:: json

  {
    "from": "conn0.console9",
    "input": "document",
    "result": {
      "type": "object",
      "className": "HTMLDocument",
      "actor": "conn0.consoleObj20"
      "extensible": true,
      "frozen": false,
      "sealed": false
    },
    "timestamp": 1347306273605,
    "exception": null,
    "exceptionMessage": null,
    "helperResult": null
  }


- ``exception`` holds the JSON-ification of the exception thrown during evaluation.
- ``exceptionMessage`` holds the ``exception.toString()`` result.
- ``result`` has the result ``ObjectActor`` instance.
- ``helperResult`` is anything that might come from a JSTerm helper result, JSON stuff (not content objects!).


.. warning::
  In Firefox 23: we renamed the ``error`` and ``errorMessage`` properties to ``exception`` and ``exceptionMessage`` respectively, to avoid conflict with the default properties used when protocol errors occur.


Autocomplete and more
---------------------

The ``autocomplete`` request packet:

.. code-block:: json

  {
    "to": "conn0.console9",
    "type": "autocomplete",
    "text": "d",
    "cursor": 1
  }


The response packet:

.. code-block:: json

  {
    "from": "conn0.console9",
    "matches": [
      "decodeURI",
      "decodeURIComponent",
      "defaultStatus",
      "devicePixelRatio",
      "disableExternalCapture",
      "dispatchEvent",
      "doMyXHR",
      "document",
      "dump"
    ],
    "matchProp": "d"
  }


There's also the ``clearMessagesCache`` request packet that has no response. This clears the console API calls cache and should clear the page errors cache - see `bug 717611 <https://bugzilla.mozilla.org/show_bug.cgi?id=717611>`_.
An alternate version was added in Firefox 104, ``clearMessagesCacheAsync``, which does exactly the same thing but resolves when the cache was actually cleared.


Network logging
***************

The ``networkEvent`` packet
---------------------------

Whenever a new network request starts being logged the ``networkEvent`` packet is sent:

.. code-block:: json

  {
    "from": "conn0.console10",
    "type": "networkEvent",
    "eventActor": {
      "actor": "conn0.netEvent14",
      "startedDateTime": "2012-09-17T19:50:03.699Z",
      "url": "http://localhost/~mihai/mozilla/test2.css",
      "method": "GET"
      "isXHR": false,
      "private": false
    }
  }


This packet is used to inform the Web Console of a new network event. For each request a new ``NetworkEventActor`` instance is created. The ``isXHR`` flag indicates if the request was initiated via an `XMLHttpRequest <https://developer.mozilla.org/en-US/docs/Web/API/XMLHttpRequest>`_ instance, that is: the ``nsIHttpChannel``'s notification is of an ``nsIXMLHttpRequest`` interface.

The ``private`` flag tells if the network request comes from a private window/tab (added in Firefox 24).


The ``NetworkEventActor``
-------------------------

The new network event actor stores further request and response information.

The ``networkEventUpdate`` packet
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Web Console UI needs to be kept up-to-date when changes happen, when new stuff is added. The new ``networkEventUpdate`` packet is sent for this purpose. Examples:

.. code-block::

  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "requestHeaders",
    "headers": 10,
    "headersSize": 425
  },
  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "requestCookies",
    "cookies": 0
  },
  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "requestPostData",
    "dataSize": 1024,
    "discardRequestBody": false
  },
  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "responseStart",
    "response": {
      "httpVersion": "HTTP/1.1",
      "status": "304",
      "statusText": "Not Modified",
      "headersSize": 194,
      "discardResponseBody": true
    }
  },
  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "eventTimings",
    "totalTime": 1
  },
  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "responseHeaders",
    "headers": 6,
    "headersSize": 194
  },
  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "responseCookies",
    "cookies": 0
  },
  {
    "from": "conn0.netEvent14",
    "type": "networkEventUpdate",
    "updateType": "responseContent",
    "mimeType": "text/css",
    "contentSize": 0,
    "discardResponseBody": true
  }


Actual headers, cookies, and bodies are not sent.


The ``getRequestHeaders`` and other packets
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To get more details about a network event you can use the following packet requests (and replies).

The ``getRequestHeaders`` packet:

.. code-block::

  {
    "to": "conn0.netEvent15",
    "type": "getRequestHeaders"
  }
  {
    "from": "conn0.netEvent15",
    "headers": [
      {
        "name": "Host",
        "value": "localhost"
      }, ...
    ],
    "headersSize": 350
  }


The ``getRequestCookies`` packet:

.. code-block:: json

  {
    "to": "conn0.netEvent15",
    "type": "getRequestCookies"
  }
  {
    "from": "conn0.netEvent15",
    "cookies": []
  }


The ``getResponseHeaders`` packet:

.. code-block::

  {
    "to": "conn0.netEvent15",
    "type": "getResponseHeaders"
  }
  {
    "from": "conn0.netEvent15",
    "headers": [
      {
        "name": "Date",
        "value": "Mon, 17 Sep 2012 20:05:27 GMT"
      }, ...
    ],
    "headersSize": 320
  }


The ``getResponseCookies`` packet:

.. code-block:: json

  {
    "to": "conn0.netEvent15",
    "type": "getResponseCookies"
  }
  {
    "from": "conn0.netEvent15",
    "cookies": []
  }


.. note::
  Starting with Firefox 19: for all of the header and cookie values in the above packets we use `LongStringActor grips <https://wiki.mozilla.org/Remote_Debugging_Protocol#Objects>`_ when the value is very long. This helps us avoid using too much of the network bandwidth.


The ``getRequestPostData`` packet:

.. code-block::

  {
    "to": "conn0.netEvent15",
    "type": "getRequestPostData"
  }
  {
    "from": "conn0.netEvent15",
    "postData": { text: "foobar" },
    "postDataDiscarded": false
  }

The ``getResponseContent`` packet:

.. code-block:: json

  {
    "to": "conn0.netEvent15",
    "type": "getResponseContent"
  }
  {
    "from": "conn0.netEvent15",
    "content": {
      "mimeType": "text/css",
      "text": "\n@import \"test.css\";\n\n.foobar { color: green }\n\n"
    },
    "contentDiscarded": false
  }


The request and response content text value is most commonly sent using a ``LongStringActor`` grip. For very short request/response bodies we send the raw text.

.. note::
  Starting with Firefox 19: for non-text response types we send the content in base64 encoding (again, most likely a ``LongStringActor`` grip). To tell the difference just check if ``response.content.encoding == "base64"``.


The ``getEventTimings`` packet:

.. code-block:: json

  {
    "to": "conn0.netEvent15",
    "type": "getEventTimings"
  }
  {
    "from": "conn0.netEvent15",
    "timings": {
      "blocked": 0,
      "dns": 0,
      "connect": 0,
      "send": 0,
      "wait": 16,
      "receive": 0
    },
    "totalTime": 16
  }


The ``fileActivity`` packet
---------------------------

When a file load is observed the following ``fileActivity`` packet is sent to the client:

.. code-block:: json

  {
    "from": "conn0.console9",
    "type": "fileActivity",
    "uri": "file:///home/mihai/public_html/mozilla/test2.css"
  }


History
*******

Protocol changes by Firefox version:

- Firefox 18: initial version.
- Firefox 19: `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=787981>`_ - added ``LongStringActor`` usage in several places.
- Firefox 20: `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=792062>`_ - removed ``locationChanged`` packet and updated the ``tabNavigated`` packet for tab actors.
- Firefox 23: `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=783499>`_ - removed the ``WebConsoleObjectActor``. Now the Web Console uses the JavaScript debugger API and the ``ObjectActor``.
- Firefox 23: added the ``bindObjectActor`` and ``frameActor`` options to the ``evaluateJS`` request packet.
- Firefox 24: new ``private`` flags for the console actor notifications, `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=874061>`_. Also added the ``lastPrivateContextExited`` notification for the global console actor.
- Firefox 24: new ``isXHR`` flag for the ``networkEvent`` notification, `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=859046>`_.
- Firefox 24: removed the ``message`` property from the ``pageError`` packet notification, `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=877773>`_. The ``lineText`` and ``errorMessage`` properties can be long string actors now.
- Firefox 25: added the ``url`` option to the ``evaluateJS`` request packet.
- Firefox 25: added the ``getPreferences`` and ``sendHTTPRequest`` request packets to the console actor, `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=886067>`_ and `bug <https://bugzilla.mozilla.org/show_bug.cgi?id=731311>`_.


Conclusions
***********

As of this writing, this document is a dense summary of the work we did in `bug 768096 <https://bugzilla.mozilla.org/show_bug.cgi?id=768096>`_ and subsequent changes. We try to keep this document up-to-date. We hope this is helpful for you.

If you make changes to the Web Console server please update this document. Thank you!
