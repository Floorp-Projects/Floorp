/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop:true */

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
  var DoNotDisturbView = sharedViews.BaseView.extend({
    template: _.template([
      '<label>',
      '  <input type="checkbox" <%- checked %>>',
      '  <span data-l10n-id="do_not_disturb"></span>',
      '</label>',
    ].join('')),

    events: {
      "click input[type=checkbox]": "toggle"
    },

    /**
     * Toggles mozLoop activation status.
     */
    toggle: function() {
      navigator.mozLoop.doNotDisturb = !navigator.mozLoop.doNotDisturb;
      this.render();
    },

    render: function() {
      this.$el.html(this.template({
        checked: navigator.mozLoop.doNotDisturb ? "checked" : ""
      }));
      return this;
    }
  });

  var ToSView = sharedViews.BaseView.extend({
    template: _.template([
      '<p data-l10n-id="legal_text_and_links"',
      '  data-l10n-args=\'',
      '    {"terms_of_use_url": "https://accounts.firefox.com/legal/terms",',
      '     "privacy_notice_url": "www.mozilla.org/privacy/"',
      '    }\'></p>'
    ].join('')),

    render: function() {
      if (navigator.mozLoop.getLoopCharPref('seenToS') === null) {
        this.$el.html(this.template());
        navigator.mozLoop.setLoopCharPref('seenToS', 'seen');
      }
      return this;
    }
  });

  /**
   * Panel view.
   */
  var PanelView = sharedViews.BaseView.extend({
    template: _.template([
      '<div class="description">',
      '  <p data-l10n-id="get_link_to_share"></p>',
      '</div>',
      '<div class="action">',
      '  <form class="invite">',
      '    <input type="text" name="caller" data-l10n-id="caller" required>',
      '    <button type="submit" class="get-url btn btn-success"',
      '       data-l10n-id="get_a_call_url"></button>',
      '  </form>',
      '  <p class="tos"></p>',
      '  <p class="result hide">',
      '    <input id="call-url" type="url" readonly>',
      '    <a class="go-back btn btn-info" href="" data-l10n-id="new_url"></a>',
      '  </p>',
      '  <p class="dnd"></p>',
      '</div>',
    ].join("")),

    className: "share generate-url",

    /**
     * Do not disturb view.
     * @type {DoNotDisturbView|undefined}
     */
    dndView: undefined,

    events: {
      "keyup input[name=caller]": "changeButtonState",
      "submit form.invite": "getCallUrl",
      "click a.go-back": "goBack"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.notifier) {
        throw new Error("missing required notifier");
      }
      this.notifier = options.notifier;
      this.client = new loop.Client();
    },

    getNickname: function() {
      return this.$("input[name=caller]").val();
    },

    getCallUrl: function(event) {
      this.notifier.clear();
      event.preventDefault();
      var callback = function(err, callUrlData) {
        this.clearPending();
        if (err) {
          this.notifier.errorL10n("unable_retrieve_url");
          this.render();
          return;
        }
        this.onCallUrlReceived(callUrlData);
      }.bind(this);

      this.setPending();
      this.client.requestCallUrl(this.getNickname(), callback);
    },

    goBack: function(event) {
      event.preventDefault();
      this.$(".action .result").hide();
      this.$(".action .invite").show();
      this.$(".description p").text(__("get_link_to_share"));
      this.changeButtonState();
    },

    onCallUrlReceived: function(callUrlData) {
      this.notifier.clear();
      this.$(".action .invite").hide();
      this.$(".action .invite input").val("");
      this.$(".action .result input").val(callUrlData.callUrl);
      this.$(".action .result").show();
      this.$(".description p").text(__("share_link_url"));
    },

    setPending: function() {
      this.$("[name=caller]").addClass("pending");
      this.$(".get-url").addClass("disabled").attr("disabled", "disabled");
    },

    clearPending: function() {
      this.$("[name=caller]").removeClass("pending");
      this.changeButtonState();
    },

    changeButtonState: function() {
      var enabled = !!this.$("input[name=caller]").val();
      if (enabled) {
        this.$(".get-url").removeClass("disabled")
            .removeAttr("disabled", "disabled");
      } else {
        this.$(".get-url").addClass("disabled").attr("disabled", "disabled");
      }
    },

    render: function() {
      this.$el.html(this.template());
      // Do not Disturb sub view
      this.dndView = new DoNotDisturbView({el: this.$(".dnd")}).render();
      this.tosView = new ToSView({el: this.$(".tos")}).render();
      return this;
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
      // purge pending notifications
      this._notifier.clear();
      // reset home view
      this.loadView(new PanelView({notifier: this._notifier}));
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
    PanelView: PanelView,
    DoNotDisturbView: DoNotDisturbView,
    PanelRouter: PanelRouter,
    ToSView: ToSView
  };
})(_, document.mozL10n);
