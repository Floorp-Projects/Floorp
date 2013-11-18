%filter substitution

%define primaryToolbarButtons #back-button, #forward-button, #home-button, #print-button, #downloads-button, #bookmarks-menu-button, #new-tab-button, #new-window-button, #cut-button, #copy-button, #paste-button, #fullscreen-button, #zoom-out-button, #zoom-reset-button, #zoom-in-button, #sync-button, #feed-button, #tabview-button, #webrtc-status-button, #social-share-button, #open-file-button, #find-button, #developer-button, #preferences-button, #privatebrowsing-button, #save-page-button, #add-ons-button, #history-panelmenu, #nav-bar-overflow-button, #PanelUI-menu-button, #characterencoding-button
%define inAnyPanel :-moz-any(:not([cui-areatype="toolbar"]),.overflowedItem)
