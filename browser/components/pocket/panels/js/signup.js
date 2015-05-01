/*
PKT_SIGNUP_OVERLAY is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/
var PKT_SIGNUP_OVERLAY = function (options) 
{
    var myself = this;
    this.baseHost = "getpocket.com";

    this.inited = false;
    this.active = false;
    this.delayedStateSaved = false;
    this.wrapper = null;
    this.variant = window.___PKT__SIGNUP_VARIANT;
    this.tagline = window.___PKT__SIGNUP_TAGLINE || '';
    this.preventCloseTimerCancel = false;
    // TODO: populate this with actual translations
    this.translations = {};
    this.closeValid = true;
    this.mouseInside = false;
    this.autocloseTimer = null;
    this.dictJSON = {};
    this.initCloseTabEvents = function() {
        $('.btn,.alreadyhave > a').click(function(e)
        {
            e.preventDefault();
            console.log('sending new tab messsage',$(this).attr('href'));
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
        else
        {
            return String(s).replace(/[&<>"']/g, function (str) {
                return sanitizeMap[str];
            });
        }
    };
    this.getTranslations = function()
    {
        var language = window.navigator.language.toLowerCase();
        this.dictJSON = {};

        var dictsuffix = 'en-US';

        if (language.indexOf('en') == 0)
        {
            dictsuffix = 'en';
        }
        else if (language.indexOf('it') == 0)
        {
            dictsuffix = 'it';
        }
        else if (language.indexOf('fr-ca') == 0)
        {
            dictsuffix = 'fr';
        }
        else if (language.indexOf('fr') == 0)
        {
            dictsuffix = 'fr';
        }
        else if (language.indexOf('de') == 0)
        {
            dictsuffix = 'de';
        }
        else if (language.indexOf('es-es') == 0)
        {
            dictsuffix = 'es';
        }
        else if (language.indexOf('es-419') == 0)
        {
            dictsuffix = 'es_419';
        }
        else if (language.indexOf('es') == 0)
        {
            dictsuffix = 'es';
        }
        else if (language.indexOf('ja') == 0)
        {
            dictsuffix = 'ja';
        }
        else if (language.indexOf('nl') == 0)
        {
            dictsuffix = 'nl';
        }
        else if (language.indexOf('pt-pt') == 0)
        {
            dictsuffix = 'pt_PT';
        }
        else if (language.indexOf('pt') == 0)
        {
            dictsuffix = 'pt_BR';
        }
        else if (language.indexOf('ru') == 0)
        {
            dictsuffix = 'ru';
        }
        else if (language.indexOf('zh-tw') == 0)
        {
            dictsuffix = 'zh_TW';
        }
        else if (language.indexOf('zh') == 0)
        {
            dictsuffix = 'zh_CN';
        }
        else if (language.indexOf('ko') == 0)
        {
            dictsuffix = 'ko';
        }
        else if (language.indexOf('pl') == 0)
        {
            dictsuffix = 'pl';
        }

        // TODO: when we add all dictionaries, modify this, but for now hard code to English
        dictsuffix = 'en';

        this.dictJSON = Translations[dictsuffix];
        
    };
};

PKT_SIGNUP_OVERLAY.prototype = {
    create : function() 
    {
        var myself = this;

        if (this.active)
        {
            return;
        }
        this.active = true;

        // set translations
        myself.getTranslations();

        // Create actual content
        $('body').append(Handlebars.templates.signup_shell(this.dictJSON));

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
        this.overlay = new PKT_SIGNUP_OVERLAY();

        this.inited = true;
    },

    addMessageListener: function(messageId, callback) {
        Messaging.addMessageListener(messageId, callback);
    },

    sendMessage: function(messageId, payload, callback) {
        Messaging.sendMessage(messageId, payload, callback);
    },

    create: function() {
        this.overlay.create();

        // tell back end we're ready
        thePKT_SIGNUP.sendMessage("show");
    }
}

$(function()
{
    if(!window.thePKT_SIGNUP){
        var thePKT_SIGNUP = new PKT_SIGNUP();
        window.thePKT_SIGNUP = thePKT_SIGNUP;
        thePKT_SIGNUP.init();
    }

    window.thePKT_SIGNUP.create();
});

