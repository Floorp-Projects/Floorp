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
        <separator class="thin"/>
        <label value="&getUserMedia.selectCamera.label;"
               accesskey="&getUserMedia.selectCamera.accesskey;"
               control="webRTC-selectCamera-menulist"/>
        <menulist id="webRTC-selectCamera-menulist">
          <menupopup id="webRTC-selectCamera-menupopup"/>
        </menulist>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-selectWindowOrScreen" orient="vertical">
        <separator class="thin"/>
        <label id="webRTC-selectWindow-label"
               control="webRTC-selectWindow-menulist"/>
        <menulist id="webRTC-selectWindow-menulist"
                  oncommand="gWebRTCUI.updateMainActionLabel(this);">
          <menupopup id="webRTC-selectWindow-menupopup"/>
        </menulist>
        <description id="webRTC-all-windows-shared" hidden="true">&getUserMedia.allWindowsShared.message;</description>
      </popupnotificationcontent>

      <popupnotificationcontent id="webRTC-selectMicrophone" orient="vertical">
        <separator class="thin"/>
        <label value="&getUserMedia.selectMicrophone.label;"
               accesskey="&getUserMedia.selectMicrophone.accesskey;"
               control="webRTC-selectMicrophone-menulist"/>
        <menulist id="webRTC-selectMicrophone-menulist">
          <menupopup id="webRTC-selectMicrophone-menupopup"/>
        </menulist>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="webapps-install-progress-notification" hidden="true">
      <popupnotificationcontent id="webapps-install-progress-content" orient="vertical" align="start">
        <separator class="thin"/>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="servicesInstall-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <!-- XXX bug 974146, tests are looking for this, can't remove yet. -->
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="pointerLock-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <separator class="thin"/>
        <label id="pointerLock-cancel" value="&pointerLock.notification.message;"/>
      </popupnotificationcontent>
    </popupnotification>

#ifdef E10S_TESTING_ONLY
    <popupnotification id="enable-e10s-notification" hidden="true">
      <popupnotificationcontent orient="vertical"/>
    </popupnotification>
#endif
