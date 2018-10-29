(function() {
  var template = Handlebars.template, templates = Handlebars.templates = Handlebars.templates || {};
templates['ho2_articleinfo'] = template({"1":function(depth0,helpers,partials,data) {
    return "pkt_ext_has_image";
},"3":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "        <div class=\"pkt_ext_save_image\" style=\"background-image:url('"
    + alias3(((helper = (helper = helpers.image_src || (depth0 != null ? depth0.image_src : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"image_src","hash":{},"data":data}) : helper)))
    + "'); background-size:cover; background-position:center center;\" data-imgsrc=\""
    + alias3(((helper = (helper = helpers.image_src || (depth0 != null ? depth0.image_src : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"image_src","hash":{},"data":data}) : helper)))
    + "\"></div>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "<div class=\"pkt_ext_experiment_saved_tile pkt_ext_cf "
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.has_image : depth0),{"name":"if","hash":{},"fn":this.program(1, data, 0),"inverse":this.noop,"data":data})) != null ? stack1 : "")
    + "\">\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.has_image : depth0),{"name":"if","hash":{},"fn":this.program(3, data, 0),"inverse":this.noop,"data":data})) != null ? stack1 : "")
    + "    <div class=\"pkt_ext_save_title\">\n        <div class=\"pkt_ext_save_open\">"
    + alias3(((helper = (helper = helpers.title || (depth0 != null ? depth0.title : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"title","hash":{},"data":data}) : helper)))
    + "</div>\n        <div class=\"pkt_ext_save_source\">"
    + alias3(((helper = (helper = helpers.domain || (depth0 != null ? depth0.domain : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"domain","hash":{},"data":data}) : helper)))
    + "</div>\n    </div>\n</div>\n";
},"useData":true});
templates['ho2_download'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var helper;

  return "<div class=\"pkt_ext_detail pkt_ext_saved_sendcollectemail\">\n    <h3 class=\"pkt_ext_heading\">Check your inbox</h3>\n    <p class=\"pkt_ext_description\">We’ve sent an email to <span class=\"pkt_ext_bold\">"
    + this.escapeExpression(((helper = (helper = helpers.email || (depth0 != null ? depth0.email : depth0)) != null ? helper : helpers.helperMissing),(typeof helper === "function" ? helper.call(depth0,{"name":"email","hash":{},"data":data}) : helper)))
    + "</span> with a link to install the Pocket app, where your article will be waiting for you.</p>\n\n    <div class=\"pkt_ext_download_section\">\n        <p class=\"pkt_ext_description\">You can also get it on the App Store here:</p>\n\n        <div class=\"pkt_ext_download_button_wrapper\">\n            <a href=\"https://getpocket.com/apps/link/pocket-iphone/?s=fx_save_hanger\" target=\"_blank\" ><div class=\"pkt_ext_apple_download\"></div></a>\n            <a href=\"https://getpocket.com/apps/link/pocket-android/?s=fx_save_hanger\" target=\"_blank\" ><img class=\"pkt_ext_google_download\" alt='Get it on Google Play' src='https://play.google.com/intl/en_us/badges/images/generic/en_badge_web_generic.png'/></a>\n        </div>\n    </div>\n</div>\n";
},"useData":true});
templates['ho2_download_error'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"pkt_ext_detail pkt_ext_saved_sendcollectemail\">\n    <h3 class=\"pkt_ext_heading\">There was a problem</h3>\n    <p class=\"pkt_ext_description\">Email failed to send, please try again later</p>\n\n    <div class=\"pkt_ext_download_section\">\n        <p class=\"pkt_ext_description\">You can also get the Pocket app on the App Store here:</p>\n\n        <div class=\"pkt_ext_download_button_wrapper\">\n            <a href=\"https://getpocket.com/apps/link/pocket-iphone/?s=fx_save_hanger\" target=\"_blank\" ><div class=\"pkt_ext_apple_download\"></div></a>\n            <a href=\"https://getpocket.com/apps/link/pocket-android/?s=fx_save_hanger\" target=\"_blank\" ><img class=\"pkt_ext_google_download\" alt='Get it on Google Play' src='https://play.google.com/intl/en_us/badges/images/generic/en_badge_web_generic.png'/></a>\n        </div>\n    </div>\n</div>\n";
},"useData":true});
templates['ho2_sharebutton_v1'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"pkt_ext_detail pkt_ext_saved_sendtomobile pkt_ext_ho2_v1\">\n    <div id=\"pkt_ext_articleinfo\">\n        <div id=\"pkt_ext_swap\" class=\"pkt_ext_experiment_saved_tile pkt_ext_has_image pkt_ext_cf\">\n            <div class=\"pkt_ext_save_image pkt_ext_save_image_placeholder\"></div>\n\n            <div class=\"pkt_ext_save_title pkt_ext_title_image_placeholder\">\n                <div class=\"pkt_ext_save_open\"> </div>\n                <div class=\"pkt_ext_save_source\"> </div>\n            </div>\n        </div>\n    </div>\n\n    <button id=\"pkt_ext_sendtomobile_button\" class=\"pkt_ext_button\">\n        <span class=\"pkt_ext_save_title_wrapper pkt_ext_mobile_icon\">\n            <span class=\"pkt_ext_logo_action_copy\">Send to your phone</span>\n        </span>\n    </button>\n</div>\n";
},"useData":true});
templates['ho2_sharebutton_v2'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"pkt_ext_detail pkt_ext_saved_sendtomobile\">\n    <button id=\"pkt_ext_sendtomobile_button\" class=\"pkt_ext_button\">\n        <span class=\"pkt_ext_save_title_wrapper pkt_ext_mobile_icon\">\n            <span class=\"pkt_ext_logo_action_copy\">Send to your phone</span>\n        </span>\n    </button>\n</div>\n";
},"useData":true});
templates['ho2_sharebutton_v3'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"pkt_ext_detail pkt_ext_saved_sendtomobile\">\n    <button id=\"pkt_ext_sendtomobile_button\" class=\"pkt_ext_button\">\n        <span class=\"pkt_ext_save_title_wrapper pkt_ext_mobile_icon\">\n            <span class=\"pkt_ext_logo_action_copy\">Get the Pocket Mobile App</span>\n        </span>\n    </button>\n</div>\n";
},"useData":true});
templates['saved_premiumextras'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"pkt_ext_suggestedtag_detailshown\">\r\n</div> ";
},"useData":true});
templates['saved_premiumshell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var helper;

  return "<div class=\"pkt_ext_suggestedtag_detail pkt_ext_suggestedtag_detail_loading\">\n    <h4>"
    + this.escapeExpression(((helper = (helper = helpers.suggestedtags || (depth0 != null ? depth0.suggestedtags : depth0)) != null ? helper : helpers.helperMissing),(typeof helper === "function" ? helper.call(depth0,{"name":"suggestedtags","hash":{},"data":data}) : helper)))
    + "</h4>\n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n    <ul class=\"pkt_ext_cf\">\n    </ul>\n</div>";
},"useData":true});
templates['saved_shell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "<div class=\"pkt_ext_initload\">\n    <div class=\"pkt_ext_logo\"></div>\n    <div class=\"pkt_ext_topdetail\">\n        <h2>"
    + alias3(((helper = (helper = helpers.saving || (depth0 != null ? depth0.saving : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"saving","hash":{},"data":data}) : helper)))
    + "</h2>\n    </div>\n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n</div>\n<div class=\"pkt_ext_detail\">\n    <div class=\"pkt_ext_logo\"></div>\n    <div class=\"pkt_ext_topdetail\">\n        <h2>"
    + alias3(((helper = (helper = helpers.pagesaved || (depth0 != null ? depth0.pagesaved : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pagesaved","hash":{},"data":data}) : helper)))
    + "</h2>\n        <h3 class=\"pkt_ext_errordetail\"></h3>\n        <nav class=\"pkt_ext_item_actions pkt_ext_cf\">\n            <ul>\n                <li><a class=\"pkt_ext_removeitem\" href=\"#\">"
    + alias3(((helper = (helper = helpers.removepage || (depth0 != null ? depth0.removepage : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"removepage","hash":{},"data":data}) : helper)))
    + "</a></li>\n                <li class=\"pkt_ext_actions_separator\"></li>\n                <li><a class=\"pkt_ext_openpocket\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/a?src=ff_ext_saved\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.viewlist || (depth0 != null ? depth0.viewlist : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"viewlist","hash":{},"data":data}) : helper)))
    + "</a></li>\n            </ul>\n        </nav>\n    </div>\n    <div class=\"pkt_ext_tag_detail pkt_ext_cf\">\n        <div class=\"pkt_ext_tag_input_wrapper\">\n            <div class=\"pkt_ext_tag_input_blocker\"></div>\n            <input class=\"pkt_ext_tag_input\" type=\"text\" placeholder=\""
    + alias3(((helper = (helper = helpers.addtags || (depth0 != null ? depth0.addtags : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"addtags","hash":{},"data":data}) : helper)))
    + "\">\n        </div>\n        <a href=\"#\" class=\"pkt_ext_btn pkt_ext_btn_disabled\">"
    + alias3(((helper = (helper = helpers.save || (depth0 != null ? depth0.save : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"save","hash":{},"data":data}) : helper)))
    + "</a>\n    </div>\n    <p class=\"pkt_ext_edit_msg\"></p>\n</div>\n";
},"useData":true});
templates['saved_tmplogin'] = template({"1":function(depth0,helpers,partials,data) {
    return "        <button id=\"pkt_ext_tmp_account_signup\" class=\"pkt_ext_button pkt_ext_blue_button\">\n            <span class=\"pkt_ext_save_title_wrapper pkt_ext_ffx_icon\">\n                <span class=\"pkt_ext_logo_action_copy\">Login with Firefox</span>\n            </span>\n        </button>\n";
},"3":function(depth0,helpers,partials,data) {
    return "        <button id=\"pkt_ext_tmp_account_signup\" class=\"pkt_ext_button\">\n            <span class=\"pkt_ext_save_title_wrapper\">\n                <span class=\"pkt_ext_logo_action_copy\">Sign up</span>\n            </span>\n        </button>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var stack1;

  return "<div class=\"pkt_ext_detail pkt_ext_saved_tmplogin pkt_shaded_background\">\n    <div class=\"pkt_ext_indent_bordered\">\n        <p>You are using Pocket without an account.<br />Sign up to back up your saved items.</p>\n        <p><a href=\"https://help.getpocket.com/article/1129-using-pocket-without-an-account-in-firefox?s=ghost_upsell\" target=\"_blank\">Learn more</a></p>\n    </div>\n\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.fxasignedin : depth0),{"name":"if","hash":{},"fn":this.program(1, data, 0),"inverse":this.program(3, data, 0),"data":data})) != null ? stack1 : "")
    + "</div>\n";
},"useData":true});
templates['signup_shell'] = template({"1":function(depth0,helpers,partials,data) {
    var stack1;

  return ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.controlvariant : depth0),{"name":"if","hash":{},"fn":this.program(2, data, 0),"inverse":this.program(4, data, 0),"data":data})) != null ? stack1 : "");
},"2":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "          <p class=\"pkt_ext_learnmorecontainer\"><a class=\"pkt_ext_learnmore\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_control&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"4":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "          <p class=\"pkt_ext_learnmorecontainer\"><a class=\"pkt_ext_learnmore\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_tryit&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"6":function(depth0,helpers,partials,data) {
    var helper;

  return "  <p class=\"pkt_ext_learnmorecontainer\"><a class=\"pkt_ext_learnmore pkt_ext_learnmoreinactive\" href=\"#\">"
    + this.escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helpers.helperMissing),(typeof helper === "function" ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"8":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "  <h4>"
    + alias3(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n  <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&t=signupff&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + alias3(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n    <p class=\"alreadyhave\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&src=extension&s=ffi&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"10":function(depth0,helpers,partials,data) {
    var stack1;

  return ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.controlvariant : depth0),{"name":"if","hash":{},"fn":this.program(11, data, 0),"inverse":this.program(13, data, 0),"data":data})) != null ? stack1 : "");
},"11":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "          <h4>"
    + alias3(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n          <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&tv=panel_control&t=signupff&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + alias3(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n            <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/signup?force=email&tv=panel_control&src=extension&s=ffi&t=signupemail&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\">"
    + alias3(((helper = (helper = helpers.signupemail || (depth0 != null ? depth0.signupemail : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupemail","hash":{},"data":data}) : helper)))
    + "</a></p>\n           <p class=\"alreadyhave\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&tv=panel_control&src=extension&s=ffi&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"13":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "          <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_tryitnow?s=ffi&tv=panel_tryit&t=tryitnow\" target=\"_blank\" class=\"btn signup-btn-tryitnow\"><span class=\"text\">"
    + alias3(((helper = (helper = helpers.tryitnow || (depth0 != null ? depth0.tryitnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"tryitnow","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n            <p class=\"alreadyhave tryitnowspace\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&s=ffi&tv=panel_tryit&src=extension&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n          <p class=\"pkt_ext_tos\">"
    + ((stack1 = ((helper = (helper = helpers.tos || (depth0 != null ? depth0.tos : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"tos","hash":{},"data":data}) : helper))) != null ? stack1 : "")
    + "</p>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var stack1, helper;

  return "<div class=\"pkt_ext_introdetail pkt_ext_introdetailhero\">\n <h2 class=\"pkt_ext_logo\">Pocket</h2>\n    <p class=\"pkt_ext_tagline\">"
    + this.escapeExpression(((helper = (helper = helpers.tagline || (depth0 != null ? depth0.tagline : depth0)) != null ? helper : helpers.helperMissing),(typeof helper === "function" ? helper.call(depth0,{"name":"tagline","hash":{},"data":data}) : helper)))
    + "</p>\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.showlearnmore : depth0),{"name":"if","hash":{},"fn":this.program(1, data, 0),"inverse":this.program(6, data, 0),"data":data})) != null ? stack1 : "")
    + " <div class=\"pkt_ext_introimg\"></div>\n</div>\n<div class=\"pkt_ext_signupdetail pkt_ext_signupdetail_hero\">\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.fxasignedin : depth0),{"name":"if","hash":{},"fn":this.program(8, data, 0),"inverse":this.program(10, data, 0),"data":data})) != null ? stack1 : "")
    + "</div>\n";
},"useData":true});
templates['signupstoryboard_shell'] = template({"1":function(depth0,helpers,partials,data) {
    var stack1;

  return ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.controlvariant : depth0),{"name":"if","hash":{},"fn":this.program(2, data, 0),"inverse":this.program(4, data, 0),"data":data})) != null ? stack1 : "");
},"2":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "                  <p><a class=\"pkt_ext_learnmore\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_control&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"4":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "                  <p><a class=\"pkt_ext_learnmore\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_tryit&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"6":function(depth0,helpers,partials,data) {
    var helper;

  return "          <p><a class=\"pkt_ext_learnmore pkt_ext_learnmoreinactive\" href=\"#\">"
    + this.escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helpers.helperMissing),(typeof helper === "function" ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"8":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "  <h4>"
    + alias3(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n  <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&t=signupff&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + alias3(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n    <p class=\"alreadyhave\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&src=extension&s=ffi&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"10":function(depth0,helpers,partials,data) {
    var stack1;

  return ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.controlvariant : depth0),{"name":"if","hash":{},"fn":this.program(11, data, 0),"inverse":this.program(13, data, 0),"data":data})) != null ? stack1 : "");
},"11":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "          <h4>"
    + alias3(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n          <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&tv=panel_control&t=signupff&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + alias3(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n            <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/signup?force=email&tv=panel_control&src=extension&s=ffi&t=signupemail&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\">"
    + alias3(((helper = (helper = helpers.signupemail || (depth0 != null ? depth0.signupemail : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupemail","hash":{},"data":data}) : helper)))
    + "</a></p>\n           <p class=\"alreadyhave\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&tv=panel_control&src=extension&s=ffi&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"13":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "          <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_tryitnow?s=ffi&tv=panel_tryit&t=tryitnow\" target=\"_blank\" class=\"btn signup-btn-tryitnow\"><span class=\"text\">"
    + alias3(((helper = (helper = helpers.tryitnow || (depth0 != null ? depth0.tryitnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"tryitnow","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n            <p class=\"alreadyhave tryitnowspace\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&s=ffi&tv=panel_tryit&src=extension&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n          <p class=\"pkt_ext_tos\">"
    + ((stack1 = ((helper = (helper = helpers.tos || (depth0 != null ? depth0.tos : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"tos","hash":{},"data":data}) : helper))) != null ? stack1 : "")
    + "</p>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "<div class=\"pkt_ext_introdetail pkt_ext_introdetailstoryboard\">\n   <div class=\"pkt_ext_introstory pkt_ext_introstoryone\">\n      <div class=\"pkt_ext_introstory_text\">\n           <p class=\"pkt_ext_tagline\">"
    + alias3(((helper = (helper = helpers.taglinestory_one || (depth0 != null ? depth0.taglinestory_one : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"taglinestory_one","hash":{},"data":data}) : helper)))
    + "</p>\n       </div>\n        <div class=\"pkt_ext_introstoryone_img\"></div>\n   </div>\n    <div class=\"pkt_ext_introstorydivider\"></div>\n   <div class=\"pkt_ext_introstory pkt_ext_introstorytwo\">\n      <div class=\"pkt_ext_introstory_text\">\n           <p class=\"pkt_ext_tagline\">"
    + alias3(((helper = (helper = helpers.taglinestory_two || (depth0 != null ? depth0.taglinestory_two : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"taglinestory_two","hash":{},"data":data}) : helper)))
    + "</p>\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.showlearnmore : depth0),{"name":"if","hash":{},"fn":this.program(1, data, 0),"inverse":this.program(6, data, 0),"data":data})) != null ? stack1 : "")
    + "     </div>\n        <div class=\"pkt_ext_introstorytwo_img\"></div>\n   </div>\n</div>\n<div class=\"pkt_ext_signupdetail\">\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.fxasignedin : depth0),{"name":"if","hash":{},"fn":this.program(8, data, 0),"inverse":this.program(10, data, 0),"data":data})) != null ? stack1 : "")
    + "\n</div>\n";
},"useData":true});
})();
