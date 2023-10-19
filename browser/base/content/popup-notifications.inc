# to be included inside a popupset element

    <panel id="notification-popup"
           type="arrow"
           position="after_start"
           hidden="true"
           orient="vertical"
           noautofocus="true"
           role="alert"/>

    <popupnotification id="webRTC-shareDevices-notification" hidden="true"
                       descriptionid="webRTC-shareDevices-notification-description">
      <popupnotificationcontent id="webRTC-selectCamera" orient="vertical">
        <label id="webRTC-selectCamera-label"
               data-l10n-id="popup-select-camera-device"
               control="webRTC-selectCamera-menulist"/>
        <html:div class="webRTC-selectDevice-selector-container">
          <xul:image id="webRTC-selectCamera-icon" class="webRTC-selectDevice-icon" data-l10n-id="popup-select-camera-icon"/>
          <menulist id="webRTC-selectCamera-menulist" aria-labelledby="webRTC-selectCamera-icon" size="large">
            <menupopup id="webRTC-selectCamera-menupopup"/>
          </menulist>
          <label id="webRTC-selectCamera-single-device-label" class="webRTC-selectDevice-label"></label>
        </html:div>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-selectWindowOrScreen" orient="vertical">
        <label id="webRTC-selectWindow-label"
               data-l10n-id="popup-select-window-or-screen"
               control="webRTC-selectWindow-menulist"/>
        <menulist id="webRTC-selectWindow-menulist"
                  oncommand="webrtcUI.updateWarningLabel(this);"
                  size="large">
          <menupopup id="webRTC-selectWindow-menupopup"/>
        </menulist>
        <description id="webRTC-all-windows-shared" hidden="true" data-l10n-id="popup-all-windows-shared"></description>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-preview" hidden="true">
        <html:video id="webRTC-previewVideo" tabindex="-1"/>
        <vbox id="webRTC-previewWarningBox">
          <description id="webRTC-previewWarning"/>
          <hbox>
            <label id="webRTC-previewWarning-learnMore" is="text-link" class="popup-notification-learnmore-link"/>
          </hbox>
        </vbox>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-selectMicrophone" orient="vertical">
        <label id="webRTC-selectMicrophone-label"
               data-l10n-id="popup-select-microphone-device"
               control="webRTC-selectMicrophone-menulist"/>
        <html:div class="webRTC-selectDevice-selector-container">
          <xul:image id="webRTC-selectMicrophone-icon" data-l10n-id="popup-select-microphone-icon" class="webRTC-selectDevice-icon"/>
          <menulist id="webRTC-selectMicrophone-menulist" aria-labelledby="webRTC-selectMicrophone-icon" size="large">
            <menupopup id="webRTC-selectMicrophone-menupopup"/>
          </menulist>
          <label id="webRTC-selectMicrophone-single-device-label" class="webRTC-selectDevice-label"></label>
        </html:div>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-selectSpeaker" orient="vertical">
        <html:div class="webRTC-selectDevice-selector-container">
          <xul:image id="webRTC-selectSpeaker-icon" data-l10n-id="popup-select-speaker-icon" class="webRTC-selectDevice-icon"/>
          <richlistbox id="webRTC-selectSpeaker-richlistbox" aria-labelledby="webRTC-selectSpeaker-icon" size="large">
          </richlistbox>
          <label id="webRTC-selectSpeaker-single-device-label" class="webRTC-selectDevice-label"></label>
        </html:div>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="servicesInstall-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <!-- XXX bug 974146, tests are looking for this, can't remove yet. -->
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="password-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <label data-l10n-id="panel-save-update-username" control="password-notification-username" class="password-notification-label"></label>
        <stack>
          <html:input id="password-notification-username" 
                      type="text"
                      class="ac-has-end-icon"
                      autocompletesearch="login-doorhanger-username"
                      autocompletepopup="PopupAutoComplete"
                      is="autocomplete-input"
                      maxrows="10"
                      maxdropmarkerrows="10"/>
          <dropmarker id="password-notification-username-dropmarker"
                      class="ac-dropmarker"/>
        </stack>
        <label data-l10n-id="panel-save-update-password" control="password-notification-password" class="password-notification-label"></label>
        <stack>
          <html:input id="password-notification-password" type="password"/>
          <dropmarker id="password-notification-password-dropmarker"
                      class="ac-dropmarker"
                      hidden="true"/>
        </stack>
        <checkbox id="password-notification-visibilityToggle" hidden="true"/>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="addon-progress-notification" is="addon-progress-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <html:progress id="addon-progress-notification-progressmeter" max="100"/>
        <label id="addon-progress-notification-progresstext" crop="end"/>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="addon-install-confirmation-notification" hidden="true">
      <popupnotificationcontent id="addon-install-confirmation-content" orient="vertical"/>
    </popupnotification>

    <popupnotification id="addon-webext-permissions-notification" hidden="true">
      <popupnotificationcontent class="addon-webext-perm-notification-content" orient="vertical">
        <description id="addon-webext-perm-text" class="addon-webext-perm-text"/>
        <label id="addon-webext-perm-intro" class="addon-webext-perm-text"/>
        <label id="addon-webext-perm-single-entry" class="addon-webext-perm-single-entry"/>
        <html:ul id="addon-webext-perm-list" class="addon-webext-perm-list"/>
        <hbox>
          <html:a
            is="moz-support-link"
            id="addon-webext-perm-info"
            class="popup-notification-learnmore-link"
            support-page="extension-permissions"
          />
        </hbox>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="addon-install-blocked-notification" hidden="true">
      <popupnotificationcontent id="addon-install-blocked-content" orient="vertical">
        <description id="addon-install-blocked-message" class="popup-notification-description"></description>
        <hbox>
          <html:a 
              is="moz-support-link"
              id="addon-install-blocked-info"
              class="popup-notification-learnmore-link"
              data-l10n-id="popup-notification-xpinstall-prompt-learn-more"
            />
        </hbox>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="canvas-permissions-prompt-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <description class="popup-notification-description"/>
        <label id="canvas-permissions-prompt-warning"/>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="contextual-feature-recommendation-notification"
                       buttonhighlight="true"
                       hidden="true">
      <popupnotificationheader id="cfr-notification-header">
        <stack id="cfr-notification-header-stack">
          <description id="cfr-notification-header-label"></description>
          <label id="cfr-notification-header-link" is="text-link">
            <xul:image id="cfr-notification-header-image"/>
          </label>
        </stack>
      </popupnotificationheader>
      <popupnotificationcontent>
      <hbox>
        <description id="cfr-notification-author"></description>
        <hbox id="cfr-notification-footer-addon-info">
            <hbox id="cfr-notification-footer-filled-stars"/>
            <hbox id="cfr-notification-footer-empty-stars"/>
            <label id="cfr-notification-footer-users"/>
          </hbox>
      </hbox>
      </popupnotificationcontent>
      <popupnotificationfooter id="cfr-notification-footer" orient="vertical">
        <vbox id="cfr-notification-footer-text-and-addon-info">
          <description id="cfr-notification-footer-text"/>
          <label id="cfr-notification-footer-learn-more-link" is="text-link"/>
        </vbox>
      </popupnotificationfooter>
    </popupnotification>

    <popupnotification id="identity-credential-notification" hidden="true">
      <popupnotificationheader id="identity-credential-header" orient="horizontal" hidden="true">
        <html:div class="identity-credential-header-container">
          <html:img class="identity-credential-header-icon"></html:img>
          <span id="identity-credential-header-text"></span>
        </html:div>
      </popupnotificationheader>
      <popupnotificationcontent id="identity-credential-provider" orient="vertical">
        <html:div id="identity-credential-provider-selector-container">
        </html:div>
        <html:template id="template-credential-provider-list-item">
          <html:label class="identity-credential-list-item" align="center">
            <html:input type="radio" name="credential-provider" class="identity-credential-list-item-radio"></html:input>
            <html:img class="identity-credential-list-item-icon" src="chrome://global/skin/icons/defaultFavicon.svg"></html:img>
            <div class="identity-credential-list-item-info">
              <span class="identity-credential-list-item-label-primary"></span>
              <span class="identity-credential-list-item-label-secondary" hidden="true"></span>
            </div>
          </html:label>
        </html:template>
      </popupnotificationcontent>
      <popupnotificationcontent id="identity-credential-policy" orient="vertical">
        <description id="identity-credential-policy-explanation" data-l10n-id="identity-credential-policy-description"  data-l10n-args='{"host":"", "provider":""}'>
          <label class="text-link" is="text-link" data-l10n-name="privacy-url" id="identity-credential-privacy-policy"></label>
          <label class="text-link" is="text-link" data-l10n-name="tos-url" id="identity-credential-terms-of-service"></label>
        </description>
      </popupnotificationcontent>
      <popupnotificationcontent id="identity-credential-account" orient="vertical" hidden="true">
        <html:div id="identity-credential-account-selector-container">
        </html:div>
        <html:template id="template-credential-account-list-item">
           <html:label class="identity-credential-list-item" align="center">
            <html:input type="radio" name="credential-account" class="identity-credential-list-item-radio"></html:input>
            <html:img class="identity-credential-list-item-icon" src="chrome://browser/skin/fxa/avatar.svg"></html:img>
            <div class="identity-credential-list-item-label-stack">
              <div class="identity-credential-list-item-label-primary"></div>
              <div class="identity-credential-list-item-label-secondary"></div>
            </div>
          </html:label>
        </html:template>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="relay-integration-offer-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <html:div>
          <html:p data-l10n-id="firefox-relay-offer-why-to-use-relay"></html:p>
          <html:p id="firefox-relay-offer-what-relay-provides" data-l10n-id="firefox-relay-offer-what-relay-provides" data-l10n-args='{"useremail": ""}'></html:p>
          <html:p id="firefox-relay-offer-legal-notice" data-l10n-id="firefox-relay-offer-legal-notice">
            <label id="firefox-relay-offer-tos-url" is="text-link" data-l10n-name="tos-url"/>
            <label id="firefox-relay-offer-privacy-url" is="text-link" data-l10n-name="privacy-url"/>
          </html:p>
        </html:div>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="relay-integration-reuse-masks-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <html:div>
          <html:p class="error-message"></html:p>
          <html:div class="reusable-relay-masks" />
        </html:div>
      </popupnotificationcontent>
    </popupnotification>

    <template id="firefox-relay-header">
      <html:div class="relay-integration-header">
        <html:div />
        <html:span>Firefox</html:span>
        <html:span> Relay</html:span>
      </html:div>
    </template>

    <popupnotification id="address-save-update-notification" class="address-capture-notification" hidden="true">
      <popupnotificationcontent class="address-save-update-notification-content" orient="vertical">
        <html:div class="address-capture-header">
          <html:p/>
        </html:div>
        <html:div class="address-capture-description">
          <html:p/>
        </html:div>
        <html:div class="address-capture-content">
        </html:div>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="address-edit-notification" class="address-capture-notification" hidden="true">
      <popupnotificationcontent class="address-edit-notification-content" orient="vertical">
        <html:div class="address-capture-header">
          <html:p/>
        </html:div>
        <html:div class="address-capture-content">
        </html:div>
      </popupnotificationcontent>
    </popupnotification>
