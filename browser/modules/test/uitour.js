/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copied from the proposed JS library for Bedrock (ie, www.mozilla.org).

// create namespace
if (typeof Mozilla == 'undefined') {
	var Mozilla = {};
}

(function($) {
  'use strict';

	// create namespace
	if (typeof Mozilla.UITour == 'undefined') {
		Mozilla.UITour = {};
	}

	var themeIntervalId = null;
	function _stopCyclingThemes() {
		if (themeIntervalId) {
			clearInterval(themeIntervalId);
			themeIntervalId = null;
		}
	}

	function _sendEvent(action, data) {
		var event = new CustomEvent('mozUITour', {
			bubbles: true,
			detail: {
				action: action,
				data: data || {}
			}
		});

		document.dispatchEvent(event);
	}

	function _generateCallbackID() {
		return Math.random().toString(36).replace(/[^a-z]+/g, '');
	}

	function _waitForCallback(callback) {
		var id = _generateCallbackID();

		function listener(event) {
			if (typeof event.detail != "object")
				return;
			if (event.detail.callbackID != id)
				return;

			document.removeEventListener("mozUITourResponse", listener);
			callback(event.detail.data);
		}
		document.addEventListener("mozUITourResponse", listener);

		return id;
	}

	Mozilla.UITour.DEFAULT_THEME_CYCLE_DELAY = 10 * 1000;

	Mozilla.UITour.registerPageID = function(pageID) {
		_sendEvent('registerPageID', {
			pageID: pageID
		});
	};

	Mozilla.UITour.showHighlight = function(target, effect) {
		_sendEvent('showHighlight', {
			target: target,
			effect: effect
		});
	};

	Mozilla.UITour.hideHighlight = function() {
		_sendEvent('hideHighlight');
	};

	Mozilla.UITour.showInfo = function(target, title, text, icon, buttons, options) {
		var buttonData = [];
		if (Array.isArray(buttons)) {
			for (var i = 0; i < buttons.length; i++) {
				buttonData.push({
					label: buttons[i].label,
					icon: buttons[i].icon,
					style: buttons[i].style,
					callbackID: _waitForCallback(buttons[i].callback)
			});
			}
		}

		var closeButtonCallbackID, targetCallbackID;
		if (options && options.closeButtonCallback)
			closeButtonCallbackID = _waitForCallback(options.closeButtonCallback);
		if (options && options.targetCallback)
			targetCallbackID = _waitForCallback(options.targetCallback);

		_sendEvent('showInfo', {
			target: target,
			title: title,
			text: text,
			icon: icon,
			buttons: buttonData,
			closeButtonCallbackID: closeButtonCallbackID,
			targetCallbackID: targetCallbackID
		});
	};

	Mozilla.UITour.hideInfo = function() {
		_sendEvent('hideInfo');
	};

	Mozilla.UITour.previewTheme = function(theme) {
		_stopCyclingThemes();

		_sendEvent('previewTheme', {
			theme: JSON.stringify(theme)
		});
	};

	Mozilla.UITour.resetTheme = function() {
		_stopCyclingThemes();

		_sendEvent('resetTheme');
	};

	Mozilla.UITour.cycleThemes = function(themes, delay, callback) {
		_stopCyclingThemes();

		if (!delay) {
			delay = Mozilla.UITour.DEFAULT_THEME_CYCLE_DELAY;
		}

		function nextTheme() {
			var theme = themes.shift();
			themes.push(theme);

			_sendEvent('previewTheme', {
				theme: JSON.stringify(theme),
				state: true
			});

			callback(theme);
		}

		themeIntervalId = setInterval(nextTheme, delay);
		nextTheme();
	};

	Mozilla.UITour.addPinnedTab = function() {
		_sendEvent('addPinnedTab');
	};

	Mozilla.UITour.removePinnedTab = function() {
		_sendEvent('removePinnedTab');
	};

	Mozilla.UITour.showMenu = function(name) {
		_sendEvent('showMenu', {
			name: name
		});
	};

	Mozilla.UITour.hideMenu = function(name) {
		_sendEvent('hideMenu', {
			name: name
		});
	};

	Mozilla.UITour.getConfiguration = function(configName, callback) {
		_sendEvent('getConfiguration', {
			callbackID: _waitForCallback(callback),
			configuration: configName,
		});
	};

	Mozilla.UITour.showFirefoxAccounts = function() {
		_sendEvent('showFirefoxAccounts');
	};

})();
