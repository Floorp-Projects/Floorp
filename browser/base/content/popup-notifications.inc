# to be included inside a popupset element

    <panel id="notification-popup"
           type="arrow"
           footertype="promobox"
           position="after_start"
           hidden="true"
           orient="vertical"
           role="alert"/>

    <!-- Popup for site identity information -->
    <panel id="identity-popup"
           type="arrow"
           hidden="true"
           noautofocus="true"
           consumeoutsideclicks="true"
           onpopupshown="gIdentityHandler.onPopupShown(event);"
           level="top">
      <hbox id="identity-popup-container" align="top">
        <image id="identity-popup-icon"/>
        <vbox id="identity-popup-content-box">
          <label id="identity-popup-connectedToLabel"
                 class="identity-popup-label"
                 value="&identity.connectedTo;"/>
          <label id="identity-popup-connectedToLabel2"
                 class="identity-popup-label"
                 value="&identity.unverifiedsite2;"/>
          <description id="identity-popup-content-host"
                       class="identity-popup-description"/>
          <label id="identity-popup-runByLabel"
                 class="identity-popup-label"
                 value="&identity.runBy;"/>
          <description id="identity-popup-content-owner"
                       class="identity-popup-description"/>
          <description id="identity-popup-content-supplemental"
                       class="identity-popup-description"/>
          <description id="identity-popup-content-verifier"
                       class="identity-popup-description"/>
          <hbox id="identity-popup-encryption" flex="1">
            <vbox>
              <image id="identity-popup-encryption-icon"/>
            </vbox>
            <description id="identity-popup-encryption-label" flex="1"
                         class="identity-popup-description"/>
          </hbox>
          <!-- Footer button to open security page info -->
          <hbox id="identity-popup-button-container" pack="end">
            <button id="identity-popup-more-info-button"
                    label="&identity.moreInfoLinkText;"
                    onblur="gIdentityHandler.hideIdentityPopup();"
                    oncommand="gIdentityHandler.handleMoreInfoClick(event);"/>
          </hbox>
        </vbox>
      </hbox>
    </panel>


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

    <popupnotification id="geolocation-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <separator class="thin"/>
        <label id="geolocation-learnmore-link" class="text-link"/>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="servicesInstall-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <separator class="thin"/>
        <label id="servicesInstall-learnmore-link" class="text-link"/>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="pointerLock-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <separator class="thin"/>
        <label id="pointerLock-cancel" value="&pointerLock.notification.message;"/>
      </popupnotificationcontent>
    </popupnotification>

    <popupnotification id="mixed-content-blocked-notification" hidden="true">
      <popupnotificationcontent orient="vertical" align="start">
        <separator/>
        <description id="mixed-content-blocked-moreinfo">&mixedContentBlocked.moreinfo;</description>
        <separator/>
        <label id="mixed-content-blocked-helplink" class="text-link"
               value="&mixedContentBlocked.helplink;"/>
      </popupnotificationcontent>
    </popupnotification>
