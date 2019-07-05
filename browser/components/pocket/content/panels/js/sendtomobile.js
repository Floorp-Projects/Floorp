/* global $:false, Handlebars:false, thePKT_SAVED:false, */

var PKT_SENDTOMOBILE = (function() {
  var width = 350;
  var ctaHeight = 200;
  var confirmHeight = 275;
  var premDetailsHeight = 110;
  var email = null;

  function _initPanelOneClicks() {
    $("#pkt_ext_sendtomobile_button").click(function() {
      $("#pkt_ext_sendtomobile_button").replaceWith(
        '<div class="pkt_ext_loadingspinner"><div></div></div>'
      );

      thePKT_SAVED.sendMessage("getMobileDownload", {}, function(data) {
        if (data.status == 1) {
          $("body").html(Handlebars.templates.ho2_download({ email }));
          thePKT_SAVED.sendMessage("resizePanel", {
            width,
            height: confirmHeight,
          });
        } else {
          $("body").html(Handlebars.templates.ho2_download_error({ email }));
          thePKT_SAVED.sendMessage("resizePanel", {
            width,
            height: confirmHeight,
          });
        }
      });
    });
  }

  function create(ho2, displayName, adjustHeight) {
    email = displayName;
    $("body").addClass("pkt_ext_ho2_experiment");
    var height = adjustHeight ? premDetailsHeight : 0;

    // Show "Send to your phone" CTA
    height += ctaHeight;
    $("body").append(Handlebars.templates.ho2_sharebutton());
    thePKT_SAVED.sendMessage("resizePanel", { width, height });

    _initPanelOneClicks();
  }

  /**
   * Public functions
   */
  return {
    create,
  };
})();
