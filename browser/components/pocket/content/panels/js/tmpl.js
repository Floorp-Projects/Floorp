(function() {
  var template = Handlebars.template, templates = Handlebars.templates = Handlebars.templates || {};
templates['variant_a'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"los_variant_wrapper\">\n  <div class=\"los_variant_top\">\n    <h1>Click the <img src=\"img/glyph.svg\" alt=\"Pocket Button\" height=\"14\" /> button to save articles, videos, and links to Pocket.</h1>\n    <p>Enjoy everything you save, on any device.</p>\n    <a class=\"pkt_ext_learnmore\" href=\"https://getpocket.com/pocket-and-firefox?utm_campaign=logged_out_save_test&utm_source=variant_a\">Learn more ›</a>\n  </div>\n\n  <div class=\"los_variant_bottom\">\n    <a class=\"los_variant_button signup-btn-email\" href=\"https://getpocket.com/signup?utm_campaign=logged_out_save_test&utm_source=variant_a\">Get Pocket for free</a>\n    <p class=\"los_variant_sub\">Already a Pocket user? <a class=\"pkt_ext_login\" href=\"https://getpocket.com/login?utm_campaign=logged_out_save_test&utm_source=variant_a\">Log in.</a></p>\n  </div>\n</div>\n";
},"useData":true});
templates['variant_b'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"los_variant_wrapper\">\n  <div class=\"los_variant_top\">\n    <h1>Here's your save button for the internet.</h1>\n    <a class=\"pkt_ext_learnmore\" href=\"https://getpocket.com/pocket-and-firefox?utm_campaign=logged_out_save_test&utm_source=variant_b\">Learn more ›</a>\n  </div>\n\n  <div class=\"los_variant_bottom\">\n    <a class=\"los_variant_button signup-btn-email\" href=\"https://getpocket.com/signup?utm_campaign=logged_out_save_test&utm_source=variant_b\">Get Pocket for free</a>\n    <p class=\"los_variant_sub\">Already a Pocket user? <a class=\"pkt_ext_login\" href=\"https://getpocket.com/login?utm_campaign=logged_out_save_test&utm_source=variant_b\">Log in.</a></p>\n  </div>\n</div>\n";
},"useData":true});
templates['variant_c'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    return "<div class=\"los_variant_wrapper\">\n  <div class=\"los_variant_top\">\n    <h1>Get Pocket to save anything to your personal corner of the internet.</h1>\n    <a class=\"pkt_ext_learnmore\" href=\"https://getpocket.com/pocket-and-firefox?utm_campaign=logged_out_save_test&utm_source=variant_c\">Learn more ›</a>\n  </div>\n\n  <div class=\"los_variant_bottom\">\n    <a class=\"los_variant_button signup-btn-email\" href=\"https://getpocket.com/signup?utm_campaign=logged_out_save_test&utm_source=variant_c\">Sign up for free</a>\n    <p class=\"los_variant_sub\">Already a Pocket user? <a class=\"pkt_ext_login\" href=\"https://getpocket.com/login?utm_campaign=logged_out_save_test&utm_source=variant_c\">Log in.</a></p>\n  </div>\n</div>\n";
},"useData":true});
templates['saved_premiumshell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var helper;

  return "<div class=\"pkt_ext_suggestedtag_detail pkt_ext_suggestedtag_detail_loading\">\n    <h4>"
    + this.escapeExpression(((helper = (helper = helpers.suggestedtags || (depth0 != null ? depth0.suggestedtags : depth0)) != null ? helper : helpers.helperMissing),(typeof helper === "function" ? helper.call(depth0,{"name":"suggestedtags","hash":{},"data":data}) : helper)))
    + "</h4>\n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n    <ul class=\"pkt_ext_cf\">\n    </ul>\n</div>\n\n<hr/>\n";
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
    + "</a>\n    </div>\n    <p class=\"pkt_ext_edit_msg\"></p>\n</div>\n\n<div class=\"pkt_ext_subshell\">\n    <div class=\"pkt_ext_item_recs\"></div>\n</div>\n";
},"useData":true});
templates['signup_shell'] = template({"1":function(depth0,helpers,partials,data) {
    var stack1;

  return ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.controlvariant : depth0),{"name":"if","hash":{},"fn":this.program(2, data, 0),"inverse":this.program(4, data, 0),"data":data})) != null ? stack1 : "");
},"2":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "					<p><a class=\"pkt_ext_learnmore\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source="
    + alias3(((helper = (helper = helpers.utmSource || (depth0 != null ? depth0.utmSource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmSource","hash":{},"data":data}) : helper)))
    + "&s=ffi&t=learnmore&tv=panel_control&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"4":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "					<p><a class=\"pkt_ext_learnmore\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source="
    + alias3(((helper = (helper = helpers.utmSource || (depth0 != null ? depth0.utmSource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmSource","hash":{},"data":data}) : helper)))
    + "&s=ffi&t=learnmore&tv=panel_tryit&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"6":function(depth0,helpers,partials,data) {
    var helper;

  return "			<p><a class=\"pkt_ext_learnmore pkt_ext_learnmoreinactive\" href=\"#\">"
    + this.escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helpers.helperMissing),(typeof helper === "function" ? helper.call(depth0,{"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"8":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "		<h4>"
    + alias3(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.buttonVariant : depth0),{"name":"if","hash":{},"fn":this.program(9, data, 0),"inverse":this.program(14, data, 0),"data":data})) != null ? stack1 : "")
    + "		<p class=\"alreadyhave\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a class=\"pkt_ext_login\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source="
    + alias3(((helper = (helper = helpers.utmSource || (depth0 != null ? depth0.utmSource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmSource","hash":{},"data":data}) : helper)))
    + "&ep=3&tv=panel_control&src=extension&s=ffi&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"9":function(depth0,helpers,partials,data) {
    var stack1;

  return ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.oneButton : depth0),{"name":"if","hash":{},"fn":this.program(10, data, 0),"inverse":this.program(12, data, 0),"data":data})) != null ? stack1 : "");
},"10":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "		    <p class=\"btn-container\">\n          <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/signup?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source=button_variant&tv=panel_control&src=extension&s=ffi&t=signupemail&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-email signup-btn-initstate\">Sign up</a>\n        </p>\n";
},"12":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "		    <p class=\"btn-container\">\n          <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source=button_control&s=ffi&tv=panel_control&t=signupff&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\">\n            <span class=\"logo\"></span><span class=\"text\">"
    + alias3(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span>\n          </a>\n        </p>\n		    <p class=\"btn-container\">\n          <a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/signup?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source=button_control&force=email&tv=panel_control&src=extension&s=ffi&t=signupemail&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\">"
    + alias3(((helper = (helper = helpers.signupemail || (depth0 != null ? depth0.signupemail : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupemail","hash":{},"data":data}) : helper)))
    + "</a>\n        </p>\n";
},"14":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "		  <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source="
    + alias3(((helper = (helper = helpers.utmSource || (depth0 != null ? depth0.utmSource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmSource","hash":{},"data":data}) : helper)))
    + "&s=ffi&tv=panel_control&t=signupff&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + alias3(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n		  <p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/signup?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source="
    + alias3(((helper = (helper = helpers.utmSource || (depth0 != null ? depth0.utmSource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmSource","hash":{},"data":data}) : helper)))
    + "&force=email&tv=panel_control&src=extension&s=ffi&t=signupemail&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\">"
    + alias3(((helper = (helper = helpers.signupemail || (depth0 != null ? depth0.signupemail : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"signupemail","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"16":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "		<p class=\"btn-container\"><a href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_tryitnow?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source="
    + alias3(((helper = (helper = helpers.utmSource || (depth0 != null ? depth0.utmSource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmSource","hash":{},"data":data}) : helper)))
    + "&s=ffi&tv=panel_tryit&t=tryitnow\" target=\"_blank\" class=\"btn signup-btn-tryitnow\"><span class=\"text\">"
    + alias3(((helper = (helper = helpers.tryitnow || (depth0 != null ? depth0.tryitnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"tryitnow","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n		<p class=\"alreadyhave tryitnowspace\">"
    + alias3(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a class=\"pkt_ext_login\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?utm_campaign="
    + alias3(((helper = (helper = helpers.utmCampaign || (depth0 != null ? depth0.utmCampaign : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmCampaign","hash":{},"data":data}) : helper)))
    + "&utm_source="
    + alias3(((helper = (helper = helpers.utmSource || (depth0 != null ? depth0.utmSource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmSource","hash":{},"data":data}) : helper)))
    + "&ep=3&s=ffi&tv=panel_tryit&src=extension&t=login&v="
    + alias3(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + alias3(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n		<p class=\"pkt_ext_tos\">"
    + ((stack1 = ((helper = (helper = helpers.tos || (depth0 != null ? depth0.tos : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"tos","hash":{},"data":data}) : helper))) != null ? stack1 : "")
    + "</p>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "<div class=\"pkt_ext_introdetail pkt_ext_introdetailstoryboard\">\n	<div class=\"pkt_ext_introstory pkt_ext_introstoryone\">\n		<div class=\"pkt_ext_introstory_text\">\n			<p class=\"pkt_ext_tagline\">"
    + alias3(((helper = (helper = helpers.taglinestory_one || (depth0 != null ? depth0.taglinestory_one : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"taglinestory_one","hash":{},"data":data}) : helper)))
    + "</p>\n		</div>\n		<div class=\"pkt_ext_introstoryone_img\"></div>\n	</div>\n	<div class=\"pkt_ext_introstorydivider\"></div>\n	<div class=\"pkt_ext_introstory pkt_ext_introstorytwo\">\n		<div class=\"pkt_ext_introstory_text\">\n			<p class=\"pkt_ext_tagline\">"
    + alias3(((helper = (helper = helpers.taglinestory_two || (depth0 != null ? depth0.taglinestory_two : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"taglinestory_two","hash":{},"data":data}) : helper)))
    + "</p>\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.showlearnmore : depth0),{"name":"if","hash":{},"fn":this.program(1, data, 0),"inverse":this.program(6, data, 0),"data":data})) != null ? stack1 : "")
    + "		</div>\n		<div class=\"pkt_ext_introstorytwo_img\"></div>\n	</div>\n</div>\n<div class=\"pkt_ext_signupdetail\">\n"
    + ((stack1 = helpers['if'].call(depth0,(depth0 != null ? depth0.controlvariant : depth0),{"name":"if","hash":{},"fn":this.program(8, data, 0),"inverse":this.program(16, data, 0),"data":data})) != null ? stack1 : "")
    + "</div>\n";
},"useData":true});
templates['home_shell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "<div class=\"pkt_ext_wrapperhome\">\n  <div class=\"pkt_ext_home\">\n    <div class=\"pkt_ext_header\">\n      <div class=\"pkt_ext_logo\"></div>\n      <a class=\"pkt_ext_mylist\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/a?src=ff_ext_home\" target=\"_blank\">\n        <span class=\"pkt_ext_mylist_icon\"></span>\n        "
    + alias3(((helper = (helper = helpers.mylist || (depth0 != null ? depth0.mylist : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"mylist","hash":{},"data":data}) : helper)))
    + "\n      </a>\n    </div>\n    <div class=\"pkt_ext_hr\"></div>\n    <div class=\"pkt_ext_detail\">\n      <h2>"
    + alias3(((helper = (helper = helpers.welcomeback || (depth0 != null ? depth0.welcomeback : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"welcomeback","hash":{},"data":data}) : helper)))
    + "</h2>\n      <p>"
    + alias3(((helper = (helper = helpers.pockethomeparagraph || (depth0 != null ? depth0.pockethomeparagraph : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethomeparagraph","hash":{},"data":data}) : helper)))
    + "</p>\n      <div class=\"pkt_ext_more\"></div>\n    </div>\n  </div>\n</div>\n";
},"useData":true});
templates['popular_topics'] = template({"1":function(depth0,helpers,partials,data) {
    var stack1, alias1=this.lambda, alias2=this.escapeExpression;

  return "    <li>\n      <a class=\"pkt_ext_topic\" href=\"https://"
    + alias2(alias1(((stack1 = (data && data.root)) && stack1.pockethost), depth0))
    + "/explore/"
    + alias2(alias1((depth0 != null ? depth0.topic : depth0), depth0))
    + "?utm_source="
    + alias2(alias1(((stack1 = (data && data.root)) && stack1.utmsource), depth0))
    + "\">\n        "
    + alias2(alias1((depth0 != null ? depth0.title : depth0), depth0))
    + "\n        <span class=\"pkt_ext_chevron_right\"></span>\n      </a>\n    </li>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var stack1, helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "<h3>"
    + alias3(((helper = (helper = helpers.explorepopulartopics || (depth0 != null ? depth0.explorepopulartopics : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"explorepopulartopics","hash":{},"data":data}) : helper)))
    + "</h3>\n<ul>\n"
    + ((stack1 = helpers.each.call(depth0,(depth0 != null ? depth0.topics : depth0),{"name":"each","hash":{},"fn":this.program(1, data, 0),"inverse":this.noop,"data":data})) != null ? stack1 : "")
    + "</ul>\n<a class=\"pkt_ext_discover\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/explore?utm_source="
    + alias3(((helper = (helper = helpers.utmsource || (depth0 != null ? depth0.utmsource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmsource","hash":{},"data":data}) : helper)))
    + "\">"
    + alias3(((helper = (helper = helpers.discovermore || (depth0 != null ? depth0.discovermore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"discovermore","hash":{},"data":data}) : helper)))
    + "</a>\n";
},"useData":true});
templates['explore_more'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var helper, alias1=helpers.helperMissing, alias2="function", alias3=this.escapeExpression;

  return "<a class=\"pkt_ext_discover\" href=\"https://"
    + alias3(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/explore?utm_source="
    + alias3(((helper = (helper = helpers.utmsource || (depth0 != null ? depth0.utmsource : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"utmsource","hash":{},"data":data}) : helper)))
    + "\">"
    + alias3(((helper = (helper = helpers.exploremore || (depth0 != null ? depth0.exploremore : depth0)) != null ? helper : alias1),(typeof helper === alias2 ? helper.call(depth0,{"name":"exploremore","hash":{},"data":data}) : helper)))
    + "</a>\n";
},"useData":true});
templates['item_recs'] = template({"1":function(depth0,helpers,partials,data) {
    return "  <h4>Similar Stories</h4>\n";
},"3":function(depth0,helpers,partials,data) {
    return "  <h4>Similar Story</h4>\n";
},"5":function(depth0,helpers,partials,data) {
    var stack1, alias1=this.lambda, alias2=this.escapeExpression;

  return "    <li>\n      <a href=\""
    + alias2(alias1(((stack1 = (depth0 != null ? depth0.item : depth0)) != null ? stack1.resolved_url : stack1), depth0))
    + "\" class=\"pkt_ext_item_recs_link\" target=\"_blank\">\n\n"
    + ((stack1 = helpers['if'].call(depth0,((stack1 = (depth0 != null ? depth0.item : depth0)) != null ? stack1.encodedThumbURL : stack1),{"name":"if","hash":{},"fn":this.program(6, data, 0),"inverse":this.noop,"data":data})) != null ? stack1 : "")
    + "\n        <p class=\"rec-title\">"
    + alias2(alias1(((stack1 = (depth0 != null ? depth0.item : depth0)) != null ? stack1.title : stack1), depth0))
    + "</p>\n        <p class=\"rec-source\">"
    + alias2(alias1(((stack1 = ((stack1 = (depth0 != null ? depth0.item : depth0)) != null ? stack1.domain_metadata : stack1)) != null ? stack1.name : stack1), depth0))
    + "</p>\n      </a>\n    </li>\n";
},"6":function(depth0,helpers,partials,data) {
    var stack1;

  return "        <img class=\"rec-thumb\" src=\"https://img-getpocket.cdn.mozilla.net/80x80/filters:format(jpeg):quality(60):no_upscale():strip_exif()/"
    + this.escapeExpression(this.lambda(((stack1 = (depth0 != null ? depth0.item : depth0)) != null ? stack1.encodedThumbURL : stack1), depth0))
    + "\" />\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
    var stack1;

  return "<header>\n"
    + ((stack1 = helpers['if'].call(depth0,((stack1 = (depth0 != null ? depth0.recommendations : depth0)) != null ? stack1['1'] : stack1),{"name":"if","hash":{},"fn":this.program(1, data, 0),"inverse":this.program(3, data, 0),"data":data})) != null ? stack1 : "")
    + "  <a class=\"pkt_ext_learn_more\" target=\"_blank\" href=\"https://getpocket.com/story_recommendations_learn_more\">Learn more</a>\n</header>\n\n<ol>\n"
    + ((stack1 = helpers.each.call(depth0,(depth0 != null ? depth0.recommendations : depth0),{"name":"each","hash":{},"fn":this.program(5, data, 0),"inverse":this.noop,"data":data})) != null ? stack1 : "")
    + "</ol>\n";
},"useData":true});
})();