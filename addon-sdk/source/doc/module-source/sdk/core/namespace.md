<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Provides an API for creating namespaces for any given objects, which
effectively may be used for creating fields that are not part of objects
public API.

      let { ns } = require('sdk/core/namespace');
      let aNamespace = ns();

      aNamespace(publicAPI).secret = secret;

One namespace may be used with multiple objects:

      let { ns } = require('sdk/core/namespace');
      let dom = ns();

      function View(element) {
        let view = Object.create(View.prototype);
        dom(view).element = element;
        // ....
      }
      View.prototype.destroy = function destroy() {
        let { element } = dom(this);
        element.parentNode.removeChild(element);
        // ...
      };
      // ...
      exports.View = View;
      // ...

Also, multiple namespaces can be used with one object:

      // ./widget.js

      let { Cu } = require('chrome');
      let { ns } = require('sdk/core/namespace');
      let { View } = require('./view');

      // Note this is completely independent from View's internal Namespace object.
      let sandboxes = ns();

      function Widget(options) {
        let { element, contentScript } = options;
        let widget = Object.create(Widget.prototype);
        View.call(widget, options.element);
        sandboxes(widget).sandbox = Cu.Sandbox(element.ownerDocument.defaultView);
        // ...
      }
      Widget.prototype = Object.create(View.prototype);
      Widget.prototype.postMessage = function postMessage(message) {
        let { sandbox } = sandboxes(this);
        sandbox.postMessage(JSON.stringify(JSON.parse(message)));
        ...
      };
      Widget.prototype.destroy = function destroy() {
        View.prototype.destroy.call(this);
        // ...
        delete sandboxes(this).sandbox;
      };
      exports.Widget = Widget;

In addition access to the namespace can be shared with other code by just
handing them a namespace accessor function.

      let { dom } = require('./view');
      Widget.prototype.setInnerHTML = function setInnerHTML(html) {
        dom(this).element.innerHTML = String(html);
      };

