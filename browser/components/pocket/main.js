/*
 * LICENSE
 *
 * POCKET MARKS
 *
 * Notwithstanding the permitted uses of the Software (as defined below) pursuant to the license set forth below, "Pocket," "Read It Later" and the Pocket icon and logos (collectively, the “Pocket Marks”) are registered and common law trademarks of Read It Later, Inc. This means that, while you have considerable freedom to redistribute and modify the Software, there are tight restrictions on your ability to use the Pocket Marks. This license does not grant you any rights to use the Pocket Marks except as they are embodied in the Software.
 *
 * ---
 *
 * SOFTWARE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Pocket UI module
 *
 * Handles interactions with Pocket buttons, panels and menus.
 *
 */

// TODO : Get the toolbar icons from Firefox's build (Nikki needs to give us a red saved icon)
// TODO : [needs clarificaiton from Fx] Firefox's plan was to hide Pocket from context menus until the user logs in. Now that it's an extension I'm wondering if we still need to do this.
// TODO : [needs clarificaiton from Fx] Reader mode (might be a something they need to do since it's in html, need to investigate their code)
// TODO : [needs clarificaiton from Fx] Move prefs within pktApi.s to sqlite or a local file so it's not editable (and is safer)
// TODO : [nice to have] - Immediately save, buffer the actions in a local queue and send (so it works offline, works like our native extensions)
// TODO : Remove console.log entries

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode", "resource://gre/modules/ReaderMode.jsm");

var pktUI = (function() {

	// -- Initialization (on startup and new windows) -- //
	var inited = false;
	var currentPanelDidShow, currentPanelDidHide;
	var _isHidden = false;
	
	var _notificationTimeout;
	
	var prefBranch = Services.prefs.getBranch("browser.pocket.settings.");
	
	/**
     * Initalizes Pocket UI and panels
     */
	function onLoad() {
		
		if (inited)
			return;
		
		// Install the button (Only on first run, if a user removes the button, you do not want to restore it)
		// TODO, only do this if in a certain language
		// TODO - Ask Mozilla what the best way is to have this update when a user restores browser defaults
		// TODO - this needs to run only on a main window - if the first run happens on a window without the toolbar, then it will never try to run it again
		if (!prefBranch.prefHasUserValue('installed')) {
		
			// If user has social add-on installed, uninstall it
			if (Social.getManifestByOrigin("https://getpocket.com")) {
				Social.uninstallProvider("https://getpocket.com", function(){ /* callback */ });
			}
			
			// if user has legacy pkt add-on installed, flag it so we can handle it correctly
			prefBranch.setBoolPref('hasLegacyExtension', hasLegacyExtension());
		
			var id = "pocket-menu-button";
	        var toolbar = document.getElementById("nav-bar");
			
	        var before = null;
			// Is the bookmarks button in the toolbar?
			if (toolbar.currentSet.match("bookmarks-menu-button")) {
	            var elem = document.getElementById("bookmarks-menu-button");
	            if (elem)
					before = elem.nextElementSibling;
			}
			// Otherwise, just add it to the end of the toolbar (because before is null)
			
        	toolbar.insertItem(id, before);
	
	        toolbar.setAttribute("currentset", toolbar.currentSet);
	        document.persist(toolbar.id, "currentset");
	        
	        prefBranch.setBoolPref('installed', true);
		}
		
		// Context Menu Event
		document.getElementById('contentAreaContextMenu').addEventListener("popupshowing", contextOnPopupShowing, false);
		
		// Hide the extension based on certain criteria
		hideIntegrationIfNeeded();
		
		inited = true;
	}
	
	/**
     * Called when window/chrome is unloaded
     */
	function onUnload() {
	
	}
	
	/**
	 * Mark all Pocket integration chrome elements as hidden if certain criteria apply (ex: legacy Pocket extension users or unsupported languages)
	 */
	function hideIntegrationIfNeeded() {
		
		var hideIntegration = false;
		
		// Check if the user had the legacy extension the last time we looked
		if (prefBranch.getBoolPref('hasLegacyExtension')) {
			if (hasLegacyExtension()) {
				hideIntegration = true; // they still have it, hide new native integration
			}
			else {
				// if they originally had it, but no longer do, then we should remove the pref so we no longer have to check
				prefBranch.setBoolPref('hasLegacyExtension', false);
			}
		}
		
		// TODO
		// If language other than launch languages (en-US currently) {
		//	hideIntegration = true;
		//}
		
		// Hide the integration if needed
		if (hideIntegration) {
			// Note, we don't hide the context menus here, that's handled in contextOnPopupShowing
			var elements = ['pocket-menu-button', 'BMB_openPocketWebapp'];
			for(var i=0; i<elements.length; i++) {
				document.getElementById(elements[i]).setAttribute('hidden', true);
			}
			
			_isHidden = true;
		}
		else
			_isHidden = false
	}
	
	
	// -- Event Handling -- //
    
    /**
     * Event handler when Pocket toolbar button is pressed
     */
    function pocketButtonOnCommand(event) {
        tryToSaveCurrentPage();
    }
    
    function pocketPanelDidShow(event) {
    	if (currentPanelDidShow) {
    		currentPanelDidShow(event);
        }
    	
    }
    
    function pocketPanelDidHide(event) {
    	if (currentPanelDidHide) {
    		currentPanelDidHide(event);
        }
        
        // clear the panel
        getPanelFrame().setAttribute('src', 'about:blank');
    }


    /**
     * Event handler when Pocket bookmark bar entry is pressed
     */
     function pocketBookmarkBarOpenPocketCommand(event) {
        openTabWithUrl('https://getpocket.com/a/', true);
     }

    /**
     * Event handler when Pocket context menu button is presed
     */

	// Determine which context menus to show before it's shown
	function contextOnPopupShowing() {

		var saveLinkId = "PKT_context_saveLink";
		var savePageId = "PKT_context_savePage";

		if (isHidden()) {
			gContextMenu.showItem(saveLinkId, false);
			gContextMenu.showItem(savePageId, false);
	    } else if ( (gContextMenu.onSaveableLink || ( gContextMenu.inDirList && gContextMenu.onLink )) ) {
			gContextMenu.showItem(saveLinkId, true);
			gContextMenu.showItem(savePageId, false);
	    } else if (gContextMenu.isTextSelected) {
			gContextMenu.showItem(saveLinkId, false);
			gContextMenu.showItem(savePageId, false);
	    } else if (!gContextMenu.onTextInput) {
			gContextMenu.showItem(saveLinkId, false);
			gContextMenu.showItem(savePageId, true);
	    } else {
			gContextMenu.showItem(saveLinkId, false);
			gContextMenu.showItem(savePageId, false);
	    }
	}

    function pocketContextSaveLinkOnCommand(event) {
    	// TODO : Unsafe CPOW Usage when saving with Page context menu (Ask Mozilla for help with this one)
        var linkNode = gContextMenu.target || document.popupNode;

        // Get parent node in case of text nodes (old safari versions)
        if (linkNode.nodeType == Node.TEXT_NODE) {
            linkNode = linkNode.parentNode;
        }

        // If for some reason, it's not an element node, abort
        if (linkNode.nodeType != Node.ELEMENT_NODE) {
            return;
        }

        // Try to get a link element in the parent chain as we can be in the
        // last child element
        var currentElement = linkNode;
        while (currentElement !== null) {
            if (currentElement.nodeType == Node.ELEMENT_NODE &&
                currentElement.nodeName.toLowerCase() == 'a')
            {
                // We have a link element try to save it
                linkNode = currentElement;
                break;
            }
            currentElement = currentElement.parentNode;
        }

        var link = linkNode.href;
        tryToSaveUrl(link);

        event.stopPropagation();
    }

    function pocketContextSavePageOnCommand(event) {
        tryToSaveCurrentPage();
    }


    // -- Communication to API -- //

    /**
     * Either save or attempt to log the user in
     */
	function tryToSaveCurrentPage() {
		tryToSaveUrl(getCurrentUrl(), getCurrentTitle());
	}
     
    function tryToSaveUrl(url, title) {

    	// If the user is logged in, go ahead and save the current page
    	if (pktApi.isUserLoggedIn()) {
    		saveAndShowConfirmation(url, title);
            return;
    	}

    	// If the user is not logged in, show the logged-out state to prompt them to authenticate
    	showSignUp();
    }


    // -- Panel UI -- //

    /**
     * Show the sign-up panel
     */
    function showSignUp() {
        getFirefoxAccountSignedInUser(function(userdata)
        {
            var fxasignedin = (typeof userdata == 'object' && userdata !== null) ? '1' : '0';
            var startheight = 490;
            if (pktApi.getSignupAB() == 'storyboard')
            {
                startheight = 460;
                if (fxasignedin == '1')
                {
                    startheight = 406;
                }
            }
            else
            {
                if (fxasignedin == '1')
                {
                    startheight = 436;
                }
            }
           showPanel("chrome://browser/content/pocket/panels/signup.html?pockethost=" + Services.prefs.getCharPref("browser.pocket.site") + "&fxasignedin=" + fxasignedin + "&variant=" + pktApi.getSignupAB(), {
               onShow: function() {
                    resizePanel({
                        width: 300,
                        height: startheight
                    });
                },
               onHide: panelDidHide,
           });
        });
    }

    /**
     * Show the logged-out state / sign-up panel
     */
    function saveAndShowConfirmation(url, title) {

        // Validate input parameter
        if (typeof url !== 'undefined' && url.startsWith("about:reader?url=")) {
            url = ReaderMode.getOriginalUrl(url);
        }

        var isValidURL = (typeof url !== 'undefined' && (url.startsWith("http") || url.startsWith('https')));

        showPanel("chrome://browser/content/pocket/panels/saved.html?pockethost=" + Services.prefs.getCharPref("browser.pocket.site") + "&premiumStatus=" + (pktApi.isPremiumUser() ? '1' : '0'), {
    		onShow: function() {
                // Open and resize the panel
                resizePanel({
                        width: 350,
                        height: 263
                });

                // Send error message for invalid url
                if (!isValidURL) {
                    var error = new Error('Only links can be saved');
                    sendErrorMessage('saveLink', error);
                    return;
                }

                // Add url
                var options = {
                    success: function(data, request) {
                        var item = data.item;
                        var successResponse = {
                            status: "success",
                            item: item
                        };
                        sendMessage('saveLink', successResponse);
                    },
                    error: function(error, request) {
                        // If user is not authorized show singup page
                        if (request.status === 401) {
                            showSignUp();
                            return;
                        }

                        // Send error message to panel
                        sendErrorMessage('saveLink', error);
                    }
                }

                // Add title if given
                if (typeof title !== "undefined") {
                    options.title = title;
                }

                // Send the link
				pktApi.addLink(url, options);
			},
			onHide: panelDidHide,
    	});
    }

    /**
     * Open a generic panel
     */
    function showPanel(url, options) {

        // We don't have to hide and show the panel again if it's already shown
        // as if the user tries to click again on the toolbar button the overlay
        // will close instead of the button will be clicked
    	var iframe = getPanelFrame();

		// Register event handlers
		registerEventMessages();

    	// Load the iframe
    	iframe.setAttribute('src', url);

    	// Uncomment to leave panel open -- for debugging
    	// panel.setAttribute('noautohide', true);
    	// panel.setAttribute('consumeoutsideclicks', false);
    	//

    	// For some reason setting onpopupshown and onpopuphidden on the panel directly didn't work, so
    	// do it this hacky way for now
    	currentPanelDidShow = options.onShow;
    	currentPanelDidHide = options.onHide;
    }

    /**
     * Resize the panel
     * options = {
     * 	width: ,
     *	height: ,
     * 	animate [default false]
     * }
     */
    function resizePanel(options) {
        var iframe = getPanelFrame();
        iframe.width = options.width;
        iframe.height = options.height;
        return;

    	// TODO : Animate the change if given options.animate = true
    	getPanel().sizeTo(options.width, options.height);
    	setTimeout(function(){
    		// we set the iframe size directly because it does not automatically stretch vertically
            var height = document.getElementById('pocket-panel-container').clientHeight + 'px';
	    	getPanelFrame().style.height = height;
	    },1);
    }

    /**
     * Called when the signup and saved panel was hidden
     */
    function panelDidHide() {
    }

    // -- Communication to Panels -- //
    // https://developer.mozilla.org/en-US/Add-ons/Code_snippets/Interaction_between_privileged_and_non-privileged_pages

    /**
     * Register a listener and callback for a specific messageId
     */
    function addMessageListener(messageId, callback) {

		document.addEventListener('PKT_'+messageId, function(e) { 
            // ignore to ensure we do not pick up other events in the browser
			if (e.target.tagName !== 'PKTMESSAGEFROMPANELELEMENT') {
				return;
            }

            // Send payload to callback
			callback(JSON.parse(e.target.getAttribute("payload"))[0]);

			// Cleanup the element
			e.target.parentNode.removeChild(e.target);

		}, false, true);

    }

    /**
     * Remove a message listener
     */
    function removeMessageListener(messageId, callback) {
    	document.removeMessageListener('PKT_'+messageId, callback);
    }

    /**
     * Send a message to the panel's iframe
     */
    function sendMessage(messageId, payload) {

    	var doc = getPanelFrame().contentWindow.document;

		var AnswerEvt = doc.createElement("PKTMessage");
	    AnswerEvt.setAttribute("payload", JSON.stringify([payload]));

	    doc.documentElement.appendChild(AnswerEvt);

	    var event = doc.createEvent("HTMLEvents");
	    event.initEvent('PKT_'+messageId, true, false);
	    AnswerEvt.dispatchEvent(event);
    }

    /**
     * Helper function to package an error object and send it to the panel iframe as a message response
     */
    function sendErrorMessage(messageId, error) {
		var errorResponse = {status: "error", error: error.message};
		sendMessage(messageId, errorResponse);
	}

    /**
     * Register all of the messages needed for the panels
     */
    function registerEventMessages() {

    	// TODO : There are likely some possible race conditions possible here, for example if the user clicks the button quickly multiple times, due to the async property of the messages, a message may be picked up for an older panel. We should consider updating this to include some sort of panelId that changes per open.
    	var iframe = getPanelFrame();

    	// Only register the messages once
    	if (iframe.getAttribute('did_init') == 1) {
    		return;
        }

    	iframe.setAttribute('did_init', 1);

		// When the panel is displayed it generated an event called
		// "show": we will listen for that event and when it happens,
		// send our own "show" event to the panel's script, so the
		// script can prepare the panel for display.
		addMessageListener("show", function(payload) {
			// Let panel know that it is ready
			sendMessage('show');
		});

        // Open a new tab with a given url and activate if 
        addMessageListener("openTabWithUrl", function(payload) {
            var activate = true;
            if (typeof payload.activate !== "undefined") {
                activate = payload.activate;
            }
            openTabWithUrl(payload.url, activate);
            sendMessage("openTabWithUrlResponse", payload.url);
        });

		// Close the panel
		addMessageListener("close", function(payload) {
			getPanel().hidePopup();
		});

		// Send the current url to the panel
		addMessageListener("getCurrentURL", function(payload) {
			sendMessage('getCurrentURLResponse', getCurrentUrl());
		});

		// Callback post initialization to tell background script that panel is "ready" for communication.
		addMessageListener("listenerReady", function(payload) {
		});

		addMessageListener("resizePanel", function(payload) {
			resizePanel(payload);
		});

		// Ask for recently accessed/used tags for auto complete
		addMessageListener("getTags", function(payload) {
			pktApi.getTags(function(tags, usedTags) {
				sendMessage('getTagsResponse', {tags, usedTags});
			});
		});

		// Ask for suggested tags based on passed url
		addMessageListener("getSuggestedTags", function(payload) {
			var responseMessageId = "getSuggestedTagsResponse";

			pktApi.getSuggestedTagsForURL(payload.url, {
				success: function(data, response) {
					var suggestedTags = data.suggested_tags;
					var successResponse = {
						status: "success",
						value: {
							"suggestedTags" : suggestedTags
						}
					}
					sendMessage(responseMessageId, successResponse);
				},
				error: function(error, response) {
					sendErrorMessage(responseMessageId, error);
				}
			})
		});

		// Pass url and array list of tags, add to existing save item accordingly
		addMessageListener("addTags", function(payload) {
			var responseMessageId = "addTagsResponse";

			pktApi.addTagsToURL(payload.url, payload.tags, {
				success: function(data, response) {
				  var successResponse = {status: "success"};
				  sendMessage(responseMessageId, successResponse);
				},
				error: function(error, response) {
				  sendErrorMessage(responseMessageId, error);
				}
			});
		});

		// Based on clicking "remove page" CTA, and passed unique item id, remove the item
		addMessageListener("deleteItem", function(payload) {
			var responseMessageId = "deleteItemResponse";

			pktApi.deleteItem(payload.itemId, {
				success: function(data, response) {
				  var successResponse = {status: "success"};
				  sendMessage(responseMessageId, successResponse);
				},
				error: function(error, response) {
					sendErrorMessage(responseMessageId, error);
				}
			})
		});
	}
	
	// -- Browser Navigation -- //
	
	/**
     * Open a new tab with a given url and notify the iframe panel that it was opened
     */

	function openTabWithUrl(url, activate) {
        var tab = gBrowser.addTab(url);
        if (activate) {
            gBrowser.selectedTab = tab;
        }
	}
    
    
    // -- Helper Functions -- //
    
    function getCurrentUrl() {
    	return getBrowser().currentURI.spec;
    }
    
    function getCurrentTitle() {
        return getBrowser().contentTitle;
    }
    
    function getPanel() {
        var frame = getPanelFrame();
        var panel = frame;
        while (panel && panel.localName != "panel") {
            panel = panel.parentNode;
        }
    	return panel;
    }
    
    function getPanelFrame() {
    	return document.getElementById('pocket-panel-iframe');
    }
    
    function hasLegacyExtension() {
    	return !!document.getElementById('RIL_urlbar_add');
    }
    
    function isHidden() {
    	return _isHidden;
    }
    
    function getFirefoxAccountSignedInUser(callback) {
       fxAccounts.getSignedInUser().then(userData => {
           callback(userData);
       }).then(null, error => {
           callback();
       });
    }
    
    /**
     * Toolbar animations
     */
    
    function showPocketAnimation() {
    	
    	// Borrowed from bookmark star animation:
    	// https://dxr.mozilla.org/mozilla-central/source/browser/base/content/browser-places.js#1568
    	
    	// TODO : Clean-up : Probably don't need all of this since the css animation does most of the heavy lifting
    	// TODO : Do not show when saving from context menu -- or get really fancy and launch the icon from the link that was saved into the bookmark menu
    
    	function getCenteringTransformForRects(rectToPosition, referenceRect) {
	      let topDiff = referenceRect.top - rectToPosition.top;
	      let leftDiff = referenceRect.left - rectToPosition.left;
	      let heightDiff = referenceRect.height - rectToPosition.height;
	      let widthDiff = referenceRect.width - rectToPosition.width;
	      return [(leftDiff + .5 * widthDiff) + "px", (topDiff + .5 * heightDiff) + "px"];
	    }
	
	    if (_notificationTimeout) {
	      clearTimeout(this._notificationTimeout);
	   	}
	
		var button = document.getElementById('pocket-menu-button');
		var bookmarksButton = document.getElementById('bookmarks-menu-button');
		var notifier = document.getElementById("pocketed-notification-anchor");
		var dropmarkerNotifier = document.getElementById("bookmarked-notification-dropmarker-anchor");
	
		
    	// If the Pocket button is not immediately after the bookmark button, then do not do the animation 
    	//	(because it's hard-coded for the positions right now)
    	// TODO - double check this in small and large toolbar button sizes
    	if (bookmarksButton.nextElementSibling != button)
    		return;
	
	    if (notifier.style.transform == '') {
	      // Get all the relevant nodes and computed style objects
	      let dropmarker = document.getAnonymousElementByAttribute(bookmarksButton, "anonid", "dropmarker");
	      let dropmarkerIcon = document.getAnonymousElementByAttribute(dropmarker, "class", "dropmarker-icon");
	      let dropmarkerStyle = getComputedStyle(dropmarkerIcon);
	      
	      // Check for RTL and get bounds
	      let isRTL = getComputedStyle(button).direction == "rtl"; // need this?
	      let buttonRect = button.getBoundingClientRect();
	      let notifierRect = notifier.getBoundingClientRect();
	      let dropmarkerRect = dropmarkerIcon.getBoundingClientRect();
	      let dropmarkerNotifierRect = dropmarkerNotifier.getBoundingClientRect();
	
	      // Compute, but do not set, transform for pocket icon
	      let [translateX, translateY] = getCenteringTransformForRects(notifierRect, buttonRect);
	      let starIconTransform = "translate(" +  (translateX) + ", " + translateY + ")";
	      if (isRTL) {
	        starIconTransform += " scaleX(-1)";
	      }
	
	      // Compute, but do not set, transform for dropmarker
	      [translateX, translateY] = getCenteringTransformForRects(dropmarkerNotifierRect, dropmarkerRect);
	      let dropmarkerTransform = "translate(" + translateX + ", " + translateY + ")";
	
	      // Do all layout invalidation in one go:
	      notifier.style.transform = starIconTransform;
	      dropmarkerNotifier.style.transform = dropmarkerTransform;
	
	      let dropmarkerAnimationNode = dropmarkerNotifier.firstChild;
	      dropmarkerAnimationNode.style.MozImageRegion = dropmarkerStyle.MozImageRegion;
	      dropmarkerAnimationNode.style.listStyleImage = dropmarkerStyle.listStyleImage;
	    }
	
	    let isInOverflowPanel = button.getAttribute("overflowedItem") == "true";
	    if (!isInOverflowPanel) {
	      notifier.setAttribute("notification", "finish");
	      button.setAttribute("notification", "finish");
	      dropmarkerNotifier.setAttribute("notification", "finish");
	    }
	
	    _notificationTimeout = setTimeout( () => {
	      notifier.removeAttribute("notification");
	      dropmarkerNotifier.removeAttribute("notification");
	      button.removeAttribute("notification");
	
	      dropmarkerNotifier.style.transform = '';
	      notifier.style.transform = '';
	    }, 1000);
    }
    
    
	/**
     * Public functions
     */
    return {
    	onLoad: onLoad,

    	pocketButtonOnCommand: pocketButtonOnCommand,
    	pocketPanelDidShow: pocketPanelDidShow,
    	pocketPanelDidHide: pocketPanelDidHide,

        pocketContextSaveLinkOnCommand,
        pocketContextSavePageOnCommand,

        pocketBookmarkBarOpenPocketCommand,

    	tryToSaveUrl: tryToSaveUrl,
    	
		isHidden
    };
}());
