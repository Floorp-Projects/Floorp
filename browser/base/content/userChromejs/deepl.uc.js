// ==UserScript==
// @name          Translate DeepL.uc.js
// @description   選択文字列をDeepLで翻訳する
// @include       main
// @charset       UTF-8
// ==/UserScript==

(
    function () {
	if (location != AppConstants.BROWSER_CHROME_URL) {
		return;
	}
	
	let translate = function () {
		let browserMM = gBrowser.selectedBrowser.messageManager;
		browserMM.addMessageListener('getSelection', function listener(message) {
			let t = (message.data !== '');
			let e = (document.charset || document.characterSet);
			if (t) {
				openWebLinkIn('https://www.deepl.com/translator#en/ja/' + encodeURIComponent(message.data), 'tab');
			} else {
				openWebLinkIn('https://www.deepl.com/translate?u=' + encodeURIComponent(gBrowser.currentURI.spec) + '&hl=ja-JA&ie=' + e + '&sl=auto&tl=ja-JA', 'tab');
			};
			browserMM.removeMessageListener('getSelection', listener, true);
		});
		browserMM.loadFrameScript('data:,sendAsyncMessage("getSelection", content.document.getSelection().toString())', true);
	}
	
	let mi = document.createXULElement('menuitem');
	mi.id = 'context-googletranslate';
	mi.setAttribute('label', 'DeepL で翻訳');
	mi.setAttribute('tooltiptext', '選択文字列をDeepLで翻訳する');
	mi.setAttribute('oncommand', '(' + translate.toString() + ')()');
	mi.classList.add('menuitem-iconic');
	mi.style.listStyleImage = ' url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAACNwAAAjcB9wZEwgAAABl0RVh0U29mdHdhcmUAd3d3Lmlua3NjYXBlLm9yZ5vuPBoAAAGZSURBVDiNjZKxaxRBFMZ/b2ZHbWITUxkRC4PnBUW0k1gkoFlMYmIR8R+w0UZBFAvtLAMS0ipC0guCd0GOa23EJGTPkBRCSCNHKiEgMzvPwmjCuiv3lft97zff7BuhQraWTouJLwE0mmf518a7spwUPxwZvlmLms8B4wWrLZhHPvuwUg44N9afGPcc4T5gK4pFgSUv/jHrre8HgKHJE4nzHWBgn/sZdBuYqQB1Q+5rbLR2DUBy1J89GIbEmNmQNW8DqxWAgcS5IQBT5vqYP3H1Gw9AHyosAKECVA4QuKfIPMhHVKygV4OVU8ByT4BDsiI6rWpusdbYUdgpBpLSMeGTRLYx2o5q9kT0ja2P3xU401ODQJz1neYdVRkT9C1gyob/Ngg/3VbifJf9TVg1L2Q43UC1eo3eb8KfB7O7uRf7T782Yo8hXBG4DFwvaRgFFoP4GTqt7u/bFuTOp5dU9BUw8u/BOhqy5fZ//4HvNL6ErHlNkSmFb4e9YM1WMV+5xjxrvM+P99VBngI/qnK96UI66OrpEhcnThatXx/tiqJJdDA6AAAAAElFTkSuQmCC")';
	let ref = document.getElementById('context-inspect');
	ref.parentNode.insertBefore(mi, ref);
})();
