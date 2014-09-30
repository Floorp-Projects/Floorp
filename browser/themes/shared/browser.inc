%filter substitution

% Note that zoom-reset-button is a bit different since it doesn't use an image and thus has the image with display: none.
%define nestedButtons #zoom-out-button, #zoom-reset-button, #zoom-in-button, #cut-button, #copy-button, #paste-button
%define primaryToolbarButtons #back-button, #forward-button, #home-button, #print-button, #downloads-button, #bookmarks-menu-button, #new-tab-button, #new-window-button, #fullscreen-button, #sync-button, #feed-button, #tabview-button, #social-share-button, #open-file-button, #find-button, #developer-button, #preferences-button, #privatebrowsing-button, #save-page-button, #switch-to-metro-button, #add-ons-button, #history-panelmenu, #nav-bar-overflow-button, #PanelUI-menu-button, #characterencoding-button, #email-link-button, #sidebar-button, @nestedButtons@, #e10s-button, #panic-button, #web-apps-button

%ifdef XP_MACOSX
% Prior to 10.7 there wasn't a native fullscreen button so we use #restore-button to exit fullscreen
% and want it to behave like other toolbar buttons.
%define primaryToolbarButtons @primaryToolbarButtons@, #restore-button
%endif

%define inAnyPanel :-moz-any(:not([cui-areatype="toolbar"]), [overflowedItem=true])
%define tabToolbarNavbarOverlap 1px
%define tabMinHeight 31px
