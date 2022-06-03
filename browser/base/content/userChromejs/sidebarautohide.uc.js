// ==UserScript==
// @name SidebarBoxAutoHide.uc.js
// @author         Griever
// @include        main
// @license        MIT License
// ==/UserScript==

(function() {
	if (location != 'chrome://browser/content/browser.xhtml') return;
	try {

//	メニューバーにメニュー挿入
		let menuitem = MozXULElement.parseXULToFragment(`
<menugroup id="sidebar-box-menu-group">
	<menuitem id="sidebar-box-showopen-menu" type="checkbox" label="常時表示" oncommand="boxautohideset(false);"/>
	<menuitem id="sidebar-box-autohide-menu" type="checkbox" label="自動開閉" oncommand="boxautohideset(true);"/>
</menugroup>
<menuseparator/>
		`);
		document.getElementById('menu_bookmarksSidebar').before(menuitem);

//	サイドバーのヘッダーにボタン作成
		let buttonitem = MozXULElement.parseXULToFragment(`
<toolbarbutton id="sidebar-box-autohide-button" class="close-icon tabbable" oncommand="boxautohideset(!document.getElementById('sidebar-box').getAttribute('boxautohide'));"/>
		`);
		document.getElementById('sidebar-close').before(buttonitem);

		let boxelm = document.getElementById('sidebar-box');
				boxelm.setAttribute('persist', 'boxautohide');

				boxelm.addEventListener('mouseenter', function() {
						boxelm.setAttribute('open', 'true');
				}, false);
				boxelm.addEventListener('mousemove', function() {
						if (!document.getElementById('sidebar-switcher-target').getAttribute('open'))
							boxelm.removeAttribute('open');
				}, false);

//	サイドバーヘッダーのメニューが開かれてたらサイドバーが閉じないようにキープ
		let menuelm = document.getElementById('sidebarMenu-popup');
				menuelm.addEventListener('popupshowing', function() {
						boxelm.setAttribute('open', 'true');
				}, false);
				menuelm.addEventListener('popuphiding', function() {
						boxelm.removeAttribute('open');
				}, false);

		let uccss = `
/* サイドバー自動開閉 */
#sidebar-box {
	z-index						: 1 !important;
}
#sidebar-splitter {
	z-index						: 1 !important;
	opacity						: 0 !important;
}
#sidebar-box #sidebar-box-autohide-button {
	list-style-image	: url("chrome://global/skin/icons/chevron.svg") !important;
}
#sidebar-box[boxautohide] #sidebar-box-autohide-button {
	list-style-image	: url("chrome://browser/skin/sidebars.svg") !important;
}
#sidebar-box[boxautohide] {
	position					: relative !important;
	overflow-x				: hidden !important;
	border						: 1px solid var(--menu-border-color) !important;
	color							: var(--arrowpanel-color) !important;
	background-color	: var(--arrowpanel-background) !important;
	box-shadow				: 4px 4px 4px -2px rgba(0, 0, 0, 0.5) !important;
	margin-top				: 8px !important;
	margin-bottom			: 8px !important;
	min-width					: 6px !important;
	max-width					: 6px !important;
	transition				: all 100ms linear 500ms !important;
	opacity						: 0 !important;
}
#sidebar-box[boxautohide]:is([open], :hover) {
	min-width					: var(--sidebar-width) !important;
	max-width					: var(--sidebar-width) !important;
	transition-delay	: 300ms !important;
	opacity						: 1 !important;
}
#sidebar-box[boxautohide]:not([positionend]) {
	margin-right			: -6px !important;
	left							: 6px !important;
}
#sidebar-box[boxautohide]:is([open], :hover):not([positionend]) {
	margin-right			: calc(var(--sidebar-width) * -1) !important;
	left							: var(--sidebar-width) !important;
	border-radius			: 0px 12px 12px 0px !important;
}
#sidebar-box[boxautohide][positionend] {
	margin-left				: -6px !important;
	right							: 6px !important;
}
#sidebar-box[boxautohide][positionend]:is([open], :hover) {
	margin-left				: calc(var(--sidebar-width) * -1) !important;
	right							: var(--sidebar-width) !important;
	border-radius			: 12px 0px 0px 12px !important;
}
		`;
		let ucuri = makeURI('data:text/css;charset=UTF=8,' + encodeURIComponent(uccss));
		let ucsss = Cc['@mozilla.org/content/style-sheet-service;1']
									.getService(Ci.nsIStyleSheetService);
				ucsss.loadAndRegisterSheet(ucuri, ucsss.AGENT_SHEET);

		boxautohideset(boxelm.getAttribute('boxautohide'));

	} catch(e) {};
})();


function boxautohideset(flag) {
	let boxelm = document.getElementById('sidebar-box');
	let width = boxelm.getAttribute('width');
	let cssvW = boxelm.style.getPropertyValue("--sidebar-width").match(/[0-9]+/);
	if (flag) {
		width = (width < 169) ? 169 : width;
		boxelm.setAttribute('boxautohide', true)
		document.getElementById('sidebar-box-autohide-button').setAttribute('tooltiptext', 'サイドバーを常に表示');
		document.getElementById('sidebar-box-autohide-menu').setAttribute('checked', 'true');
		document.getElementById('sidebar-box-showopen-menu').removeAttribute('checked');
	} else {
		width = (cssvW < 169) ? 169 : cssvW;
		boxelm.removeAttribute('boxautohide');
		document.getElementById('sidebar-box-autohide-button').setAttribute('tooltiptext', 'サイドバーを自動開閉');
		document.getElementById('sidebar-box-autohide-menu').removeAttribute('checked');
		document.getElementById('sidebar-box-showopen-menu').setAttribute('checked', 'true');
	}
	boxelm.setAttribute('width', width);
	boxelm.style.setProperty("--sidebar-width", width + "px");
}

