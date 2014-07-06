/** @jsx React.DOM */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint newcap:false*/
/*global loop:true, React */

var loop = loop || {};
loop.panel = (function(_, mozL10n) {
  "use strict";

  var sharedViews = loop.shared.views,
      // aliasing translation function as __ for concision
      __ = mozL10n.get;

  /**
   * Panel router.
   * @type {loop.desktopRouter.DesktopRouter}
   */
  var router;

  /**
   * Do not disturb panel subview.
   */
  var DoNotDisturb = React.createClass({displayName: 'DoNotDisturb',
    getInitialState: function() {
      return {doNotDisturb: navigator.mozLoop.doNotDisturb};
    },

    handleCheckboxChange: function() {
      // Note: side effect!
      navigator.mozLoop.doNotDisturb = !navigator.mozLoop.doNotDisturb;
      this.setState({doNotDisturb: navigator.mozLoop.doNotDisturb});
    },

    render: function() {
      // XXX https://github.com/facebook/react/issues/310 for === htmlFor
      return (
        React.DOM.p( {className:"dnd"}, 
          React.DOM.input( {type:"checkbox", checked:this.state.doNotDisturb,
                 id:"dnd-component", onChange:this.handleCheckboxChange} ),
          React.DOM.label( {htmlFor:"dnd-component"}, __("do_not_disturb"))
        )
      );
    }
  });

  var ToSView = React.createClass({displayName: 'ToSView',
    getInitialState: function() {
      return {seenToS: navigator.mozLoop.getLoopCharPref('seenToS')};
    },

    render: function() {
      var tosHTML = __("legal_text_and_links", {
        "terms_of_use_url": "https://accounts.firefox.com/legal/terms",
        "privacy_notice_url": "www.mozilla.org/privacy/"
      });

      if (!this.state.seenToS) {
        navigator.mozLoop.setLoopCharPref('seenToS', 'seen');
        return React.DOM.p( {className:"tos",
                  dangerouslySetInnerHTML:{__html: tosHTML}});
      } else {
        return React.DOM.div(null );
      }
    }
  });

  var PanelLayout = React.createClass({displayName: 'PanelLayout',
    propTypes: {
      summary: React.PropTypes.string.isRequired
    },

    render: function() {
      return (
        React.DOM.div( {className:"share generate-url"}, 
          React.DOM.div( {className:"description"}, 
            React.DOM.p(null, this.props.summary)
          ),
          React.DOM.div( {className:"action"}, 
            this.props.children
          )
        )
      );
    }
  });

  var CallUrlResult = React.createClass({displayName: 'CallUrlResult',
    propTypes: {
      callUrl: React.PropTypes.string.isRequired,
      retry: React.PropTypes.func.isRequired
    },

    handleButtonClick: function() {
      this.props.retry();
    },

    render: function() {
      // XXX setting elem value from a state (in the callUrl input)
      // makes it immutable ie read only but that is fine in our case.
      // readOnly attr will suppress a warning regarding this issue
      // from the react lib.
      return (
        PanelLayout( {summary:__("share_link_url")}, 
          React.DOM.div( {className:"invite"}, 
            React.DOM.input( {type:"url", value:this.props.callUrl, readOnly:"true"} ),
            React.DOM.button( {onClick:this.handleButtonClick,
                    className:"btn btn-success"}, __("new_url"))
          )
        )
      );
    }
  });

  var CallUrlForm = React.createClass({displayName: 'CallUrlForm',
    propTypes: {
      client: React.PropTypes.object.isRequired,
      notifier: React.PropTypes.object.isRequired
    },

    getInitialState: function() {
      return {
        pending: false,
        disabled: true,
        callUrl: false
      };
    },

    retry: function() {
      this.setState(this.getInitialState());
    },

    handleTextChange: function(event) {
      this.setState({disabled: !event.currentTarget.value});
    },

    handleFormSubmit: function(event) {
      event.preventDefault();

      this.setState({pending: true});

      this.props.client.requestCallUrl(
        this.refs.caller.getDOMNode().value, this._onCallUrlReceived);
    },

    _onCallUrlReceived: function(err, callUrlData) {
      var callUrl = false;

      this.props.notifier.clear();

      if (err) {
        this.props.notifier.errorL10n("unable_retrieve_url");
      } else {
        callUrl = callUrlData.callUrl || callUrlData.call_url;
      }

      this.setState({pending: false, callUrl: callUrl});
    },

    render: function() {
      // If we have a call url, render result
      if (this.state.callUrl) {
        return (
          CallUrlResult( {callUrl:this.state.callUrl, retry:this.retry})
        );
      }

      // If we don't display the form
      var cx = React.addons.classSet;
      return (
        PanelLayout( {summary:__("get_link_to_share")}, 
          React.DOM.form( {className:"invite", onSubmit:this.handleFormSubmit}, 

            React.DOM.input( {type:"text", name:"caller", ref:"caller", required:"required",
                   className:cx({'pending': this.state.pending}),
                   onChange:this.handleTextChange,
                   placeholder:__("call_identifier_textinput_placeholder")} ),

            React.DOM.button( {type:"submit", className:"get-url btn btn-success",
                    disabled:this.state.disabled}, 
              __("get_a_call_url")
            )
          ),
          ToSView(null )
        )
      );
    }
  });

  /**
   * Panel view.
   */
  var PanelView = React.createClass({displayName: 'PanelView',
    propTypes: {
      notifier: React.PropTypes.object.isRequired,
      client: React.PropTypes.object.isRequired
    },

    render: function() {
      return (
        React.DOM.div(null, 
          CallUrlForm( {client:this.props.client,
                       notifier:this.props.notifier} ),
          DoNotDisturb(null )
        )
      );
    }
  });

  var PanelRouter = loop.desktopRouter.DesktopRouter.extend({
    /**
     * DOM document object.
     * @type {HTMLDocument}
     */
    document: undefined,

    routes: {
      "": "home"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.document) {
        throw new Error("missing required document");
      }
      this.document = options.document;

      this._registerVisibilityChangeEvent();

      this.on("panel:open panel:closed", this.reset, this);
    },

    /**
     * Register the DOM visibility API event for the whole document, and trigger
     * appropriate events accordingly:
     *
     * - `panel:opened` when the panel is open
     * - `panel:closed` when the panel is closed
     *
     * @link  http://www.w3.org/TR/page-visibility/
     */
    _registerVisibilityChangeEvent: function() {
      this.document.addEventListener("visibilitychange", function(event) {
        this.trigger(event.currentTarget.hidden ? "panel:closed"
                                                : "panel:open");
      }.bind(this));
    },

    /**
     * Default entry point.
     */
    home: function() {
      this.reset();
    },

    /**
     * Resets this router to its initial state.
     */
    reset: function() {
      this._notifier.clear();
      var client = new loop.Client({
        baseServerUrl: navigator.mozLoop.serverUrl
      });
      this.loadReactComponent(PanelView( {client:client,
                                         notifier:this._notifier} ));
    }
  });

  /**
   * Panel initialisation.
   */
  function init() {
    // Do the initial L10n setup, we do this before anything
    // else to ensure the L10n environment is setup correctly.
    mozL10n.initialize(navigator.mozLoop);

    router = new PanelRouter({
      document: document,
      notifier: new sharedViews.NotificationListView({el: "#messages"})
    });
    Backbone.history.start();

    // Notify the window that we've finished initalization and initial layout
    var evtObject = document.createEvent('Event');
    evtObject.initEvent('loopPanelInitialized', true, false);
    window.dispatchEvent(evtObject);
  }

  return {
    init: init,
    DoNotDisturb: DoNotDisturb,
    CallUrlForm: CallUrlForm,
    PanelView: PanelView,
    PanelRouter: PanelRouter,
    ToSView: ToSView
  };
})(_, document.mozL10n);
