# to be included inside a popupset element

    <panel id="notification-popup"
           type="arrow"
           position="after_start"
           hidden="true"
           orient="vertical"
           noautofocus="true"
           role="alert"/>

    <popupnotification id="webRTC-shareDevices-notification" hidden="true">
      <popupnotificationcontent id="webRTC-selectCamera" orient="vertical">
        <label value="&getUserMedia.selectCamera.label;"
               accesskey="&getUserMedia.selectCamera.accesskey;"
               control="webRTC-selectCamera-menulist"/>
        <menulist id="webRTC-selectCamera-menulist">
          <menupopup id="webRTC-selectCamera-menupopup"/>
        </menulist>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-selectWindowOrScreen" orient="vertical">
        <label id="webRTC-selectWindow-label"
               control="webRTC-selectWindow-menulist"/>
        <menulist id="webRTC-selectWindow-menulist"
                  oncommand="webrtcUI.updateWarningLabel(this);">
          <menupopup id="webRTC-selectWindow-menupopup"/>
        </menulist>
        <description id="webRTC-all-windows-shared" hidden="true">&getUserMedia.allWindowsShared.message;</description>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-preview" hidden="true">
        <html:video id="webRTC-previewVideo"/>
        <vbox id="webRTC-previewWarningBox">
          <spacer flex="1"/>
          <description id="webRTC-previewWarning"/>
        </vbox>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-selectMicrophone" orient="vertical">
        <label value="&getUserMedia.selectMicrophone.label;"
               accesskey="&getUserMedia.selectMicrophone.accesskey;"
               control="webRTC-selectMicrophone-menulist"/>
        <menulist id="webRTC-selectMicrophone-menulist">
          <menupopup id="webRTC-selectMicrophone-menupopup"/>
        </menulist>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="servicesInstall-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <!-- XXX bug 974146, tests are looking for this, can't remove yet. -->
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="password-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <textbox id="password-notification-username"/>
        <textbox id="password-notification-password" type="password" show-content=""/>
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
        <html:ul id="addon-webext-perm-list" class="addon-webext-perm-list"/>
        <hbox>
          <label id="addon-webext-perm-info" is="text-link" class="popup-notification-learnmore-link"/>
        </hbox>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="addon-install-blocked-notification" hidden="true">
      <popupnotificationcontent id="addon-install-blocked-content" orient="vertical">
        <description id="addon-install-blocked-message" class="popup-notification-description"></description>
        <hbox>
          <label id="addon-install-blocked-info" class="popup-notification-learnmore-link" is="text-link"/>
        </hbox>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="contextual-feature-recommendation-notification" hidden="true">
      <popupnotificationheader id="cfr-notification-header">
        <stack id="cfr-notification-header-stack">
          <description id="cfr-notification-header-label"></description>
          <label id="cfr-notification-header-link" is="text-link">
            <xul:image id="cfr-notification-header-image"/>
          </label>
        </stack>
      </popupnotificationheader>
      <popupnotificationcontent>
        <description id="cfr-notification-author"></description>
      </popupnotificationcontent>
      <popupnotificationfooter id="cfr-notification-footer" orient="vertical">
        <vbox id="cfr-notification-footer-text-and-addon-info">
          <description id="cfr-notification-footer-text"/>
          <hbox id="cfr-notification-footer-addon-info">
            <hbox id="cfr-notification-footer-filled-stars"/>
            <hbox id="cfr-notification-footer-empty-stars"/>
            <label id="cfr-notification-footer-users"/>
            <spacer id="cfr-notification-footer-spacer" hidden="true"/>
            <label id="cfr-notification-footer-learn-more-link" is="text-link"/>
          </hbox>
        </vbox>
      </popupnotificationfooter>
    </popupnotification>

    <popupnotification id="storage-access-notification" hidden="true">
      <popupnotificationcontent class="storage-access-notification-content">
        <xul:vbox flex="1">
          <!-- These need to be on the same line to avoid creating
               whitespace between them (whitespace is added in the
               localization file, if necessary). -->
          <xul:description class="storage-access-perm-text"><html:span
            id="storage-access-perm-label"/><html:a id="storage-access-perm-learnmore"
            onclick="openTrustedLinkIn(this.href, 'tab'); return false;"
            class="text-link popup-notification-learnmore-link"/><html:span
            id="storage-access-perm-endlabel"/></xul:description>
        </xul:vbox>
      </popupnotificationcontent>
    </popupnotification>
