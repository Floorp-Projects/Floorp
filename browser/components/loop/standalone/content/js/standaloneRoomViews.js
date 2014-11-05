/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true, React */

var loop = loop || {};
loop.standaloneRoomViews = (function() {
  "use strict";

  var StandaloneRoomView = React.createClass({displayName: 'StandaloneRoomView',
    render: function() {
      return (React.DOM.div(null, "Room"));
    }
  });

  return {
    StandaloneRoomView: StandaloneRoomView
  };
})();
