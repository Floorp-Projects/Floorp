/*
PKT_SIGNUP_OVERLAY is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/
var PKT_SIGNUP_OVERLAY = function (options)
{
    var myself = this;
    this.inited = false;
    this.active = false;
    this.delayedStateSaved = false;
    this.wrapper = null;
    this.variant = window.___PKT__SIGNUP_VARIANT;
    this.tagline = window.___PKT__SIGNUP_TAGLINE || '';
    this.preventCloseTimerCancel = false;
    this.translations = {};
    this.closeValid = true;
    this.mouseInside = false;
    this.autocloseTimer = null;
    this.variant = "";
    this.inoverflowmenu = false;
    this.controlvariant;
    this.pockethost = "getpocket.com";
    this.fxasignedin = false;
    this.dictJSON = {};
    this.initCloseTabEvents = function() {
        $('.btn,.pkt_ext_learnmore,.alreadyhave > a').click(function(e)
        {
            e.preventDefault();
            thePKT_SIGNUP.sendMessage("openTabWithUrl",
            {
                url: $(this).attr('href'),
                activate: true
            });
            myself.closePopup();
        });
    };
    this.closePopup = function() {
        thePKT_SIGNUP.sendMessage("close");
    };
    this.sanitizeText = function(s) {
        var sanitizeMap = {
            "&": "&amp;",
            "<": "&lt;",
            ">": "&gt;",
            '"': '&quot;',
            "'": '&#39;'
        };
        if (typeof s !== 'string')
        {
            return '';
        }
        return String(s).replace(/[&<>"']/g, function (str) {
            return sanitizeMap[str];
        });
    };
    this.getTranslations = function()
    {
        this.dictJSON = window.pocketStrings;
    };

};

PKT_SIGNUP_OVERLAY.prototype = {
    create : function()
    {
        var myself = this;

        var controlvariant = window.location.href.match(/controlvariant=([\w|\.]*)&?/);
        if (controlvariant && controlvariant.length > 1)
        {
            this.controlvariant = controlvariant[1];
        }
        var variant = window.location.href.match(/variant=([\w|\.]*)&?/);
        if (variant && variant.length > 1)
        {
            this.variant = variant[1];
        }
        var fxasignedin = window.location.href.match(/fxasignedin=([\w|\d|\.]*)&?/);
        if (fxasignedin && fxasignedin.length > 1)
        {
            this.fxasignedin = (fxasignedin[1] == '1');
        }
        var host = window.location.href.match(/pockethost=([\w|\.]*)&?/);
        if (host && host.length > 1)
        {
            this.pockethost = host[1];
        }
        var inoverflowmenu = window.location.href.match(/inoverflowmenu=([\w|\.]*)&?/);
        if (inoverflowmenu && inoverflowmenu.length > 1)
        {
            this.inoverflowmenu = (inoverflowmenu[1] == 'true');
        }
        var locale = window.location.href.match(/locale=([\w|\.]*)&?/);
        if (locale && locale.length > 1)
        {
           this.locale = locale[1].toLowerCase();
        }

        if (this.active)
        {
            return;
        }
        this.active = true;

        // set translations
        this.getTranslations();
        this.dictJSON.fxasignedin = this.fxasignedin ? 1 : 0;
        this.dictJSON.controlvariant = this.controlvariant == 'true' ? 1 : 0;
        this.dictJSON.variant = (this.variant ? this.variant : 'undefined');
        this.dictJSON.variant += this.fxasignedin ? '_fxa' : '_nonfxa';
        this.dictJSON.pockethost = this.pockethost;
        this.dictJSON.showlearnmore = true;

        // extra modifier class for collapsed state
        if (this.inoverflowmenu)
        {
            $('body').addClass('pkt_ext_signup_overflow');
        }

        // extra modifier class for language
        if (this.locale)
        {
            $('body').addClass('pkt_ext_signup_' + this.locale);
        }

        // Create actual content
        if (this.variant == 'overflow')
        {
            $('body').append(Handlebars.templates.signup_shell(this.dictJSON));
        }
        else
        {
            $('body').append(Handlebars.templates.signupstoryboard_shell(this.dictJSON));
        }


        // tell background we're ready
        thePKT_SIGNUP.sendMessage("show");

        // close events
        this.initCloseTabEvents();
    }
};


// Layer between Bookmarklet and Extensions
var PKT_SIGNUP = function () {};

PKT_SIGNUP.prototype = {
    init: function () {
        if (this.inited) {
            return;
        }
        this.panelId = pktPanelMessaging.panelIdFromURL(window.location.href);
        this.overlay = new PKT_SIGNUP_OVERLAY();

        this.inited = true;
    },

    addMessageListener: function(messageId, callback) {
        pktPanelMessaging.addMessageListener(this.panelId, messageId, callback);
    },

    sendMessage: function(messageId, payload, callback) {
        pktPanelMessaging.sendMessage(this.panelId, messageId, payload, callback);
    },

    create: function() {
        this.overlay.create();

        // tell back end we're ready
        thePKT_SIGNUP.sendMessage("show");
    }
}

$(function()
{
    if (!window.thePKT_SIGNUP) {
        var thePKT_SIGNUP = new PKT_SIGNUP();
        window.thePKT_SIGNUP = thePKT_SIGNUP;
        thePKT_SIGNUP.init();
    }

    var pocketHost = thePKT_SIGNUP.overlay.pockethost;
    // send an async message to get string data
    thePKT_SIGNUP.sendMessage("initL10N", {
            tos: [
                'https://'+ pocketHost +'/tos?s=ffi&t=tos&tv=panel_tryit',
                'https://'+ pocketHost +'/privacy?s=ffi&t=privacypolicy&tv=panel_tryit'
            ]
        }, function(resp) {
        window.pocketStrings = resp.strings;
        window.thePKT_SIGNUP.create();
    });
});

