# to be included inside a popupset element

    <panel id="notification-popup"
           type="arrow"
           footertype="promobox"
           position="after_start"
           hidden="true"
           orient="vertical"
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
                  oncommand="gWebRTCUI.updateMainActionLabel(this);">
          <menupopup id="webRTC-selectWindow-menupopup"/>
        </menulist>
        <description id="webRTC-all-windows-shared" hidden="true">&getUserMedia.allWindowsShared.message;</description>
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

    <popupnotification id="webapps-install-progress-notification" hidden="true">
      <popupnotificationcontent id="webapps-install-progress-content" orient="vertical" align="start"/>
    </popupnotification>

    <popupnotification id="servicesInstall-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <!-- XXX bug 974146, tests are looking for this, can't remove yet. -->
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="pointerLock-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <label id="pointerLock-cancel">&pointerLock.notification.message;</label>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="password-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <textbox id="password-notification-username"/>
        <textbox id="password-notification-password" type="password"
                 disabled="true"/>
      </popupnotificationcontent>
    </popupnotification>

    <vbox id="login-fill-doorhanger" hidden="true">
      <description id="login-fill-testing"
                   value="Thanks for testing the login fill doorhanger!"/>
      <textbox id="login-fill-filter"/>
      <richlistbox id="login-fill-list"/>
    </vbox>

#ifdef E10S_TESTING_ONLY
    <popupnotification id="enable-e10s-notification" hidden="true">
      <popupnotificationcontent orient="vertical"/>
    </popupnotification>
#endif

    <popupnotification id="addon-progress-notification" hidden="true">
      <popupnotificationcontent orient="vertical">
        <progressmeter id="addon-progress-notification-progressmeter"/>
        <label id="addon-progress-notification-progresstext" crop="end"/>
      </popupnotificationcontent>
      <button id="addon-progress-cancel"
              oncommand="this.parentNode.cancel();"/>
      <button id="addon-progress-accept" disabled="true"/>
    </popupnotification>

    <popupnotification id="addon-install-confirmation-notification" hidden="true">
      <popupnotificationcontent id="addon-install-confirmation-content" orient="vertical"/>
      <button id="addon-install-confirmation-cancel"
              oncommand="PopupNotifications.getNotification('addon-install-confirmation').remove();"/>
      <button id="addon-install-confirmation-accept"
              oncommand="gXPInstallObserver.acceptInstallation();
                         PopupNotifications.getNotification('addon-install-confirmation').remove();"/>
    </popupnotification>
