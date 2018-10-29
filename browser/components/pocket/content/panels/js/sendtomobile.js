/* global $:false, Handlebars:false, thePKT_SAVED:false, */

var PKT_SENDTOMOBILE = (function() {

    var width = 350;
    var variant1Height = 312;
    var variant2Height = 200;
    var confirmHeight = 275;
    var premDetailsHeight = 110;
    var email = null;

    function _swapPlaceholder(data) {
        var info = {};

        if (!data.item_preview) {
            info.domain = data.fallback_domain;
            info.title = data.fallback_title;
        } else {
            info.domain = data.item_preview.resolved_domain;
            info.title = data.item_preview.title;
            info.has_image = (data.item_preview.top_image_url) ? 1 : 0;

            if (data.item_preview.top_image_url) {
                info.image_src = getImageCacheUrl(data.item_preview.top_image_url, "w225");
            }
        }

        $("#pkt_ext_swap").replaceWith(Handlebars.templates.ho2_articleinfo(info));
    }

    function _initPanelOneClicks() {
        $("#pkt_ext_sendtomobile_button").click(function() {
            $("#pkt_ext_sendtomobile_button").replaceWith("<div class=\"pkt_ext_loadingspinner\"><div></div></div>");

            thePKT_SAVED.sendMessage("getMobileDownload", {}, function(data) {
                if (data.status == 1) {
                    $("body").html(Handlebars.templates.ho2_download({email}));
                    thePKT_SAVED.sendMessage("resizePanel", { width, height: confirmHeight });
                } else {
                    $("body").html(Handlebars.templates.ho2_download_error({email}));
                    thePKT_SAVED.sendMessage("resizePanel", { width, height: confirmHeight });
                }
            });
        });
    }

    function create(ho2, displayName, adjustHeight) {
        email = displayName;
        $("body").addClass("pkt_ext_ho2_experiment");
        var height = (adjustHeight) ? premDetailsHeight : 0;

        if (ho2 == "show_prompt_preview") {
            height += variant1Height;
            $("body").append(Handlebars.templates.ho2_sharebutton_v1());
            thePKT_SAVED.sendMessage("resizePanel", { width, height });
            thePKT_SAVED.sendMessage("getArticleInfo", {}, function(data) {
                _swapPlaceholder(data);
            });
        } else if (ho2 == "show_prompt_no_preview") {
            height += variant2Height;
            $("body").append(Handlebars.templates.ho2_sharebutton_v2());
            thePKT_SAVED.sendMessage("resizePanel", { width, height });
        } else if (ho2 == "show_prompt_get_app") {
            height += variant2Height;
            $("body").append(Handlebars.templates.ho2_sharebutton_v3());
            thePKT_SAVED.sendMessage("resizePanel", { width, height });
        }

        _initPanelOneClicks();
    }

    /**
     * Public functions
     */
    return {
        create,
    };
}());


/**
 * This function is based on getImageCacheUrl in:
 * https://github.com/Pocket/Web/blob/master/public_html/a/j/shared.js
 */
function getImageCacheUrl(url, resize, fallback) {
    if (!url)
        return false;
    // The full URL should be included in the get parameters such that the image cache can request it.
    var query = {
        "url": url,
    };
    if (resize)
        query.resize = resize;
    if (fallback)
        query.f = fallback;

    return "https://d33ypg4xwx0n86.cloudfront.net/direct?" + $.param(query);
}
