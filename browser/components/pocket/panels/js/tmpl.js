(function() {
  var template = Handlebars.template, templates = Handlebars.templates = Handlebars.templates || {};
templates['saved_premiumshell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "<div class=\"pkt_ext_suggestedtag_detail pkt_ext_suggestedtag_detail_loading\">\n    <h4>"
    + escapeExpression(((helper = (helper = helpers.suggestedtags || (depth0 != null ? depth0.suggestedtags : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"suggestedtags","hash":{},"data":data}) : helper)))
    + "</h4>\n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n    <ul class=\"pkt_ext_cf\">\n    </ul>\n</div>";
},"useData":true});
templates['saved_shell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "<div class=\"pkt_ext_initload\">                        \n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n</div>                                      \n<div class=\"pkt_ext_detail\">                        \n    <div class=\"pkt_ext_logo\"></div>\n    <div class=\"pkt_ext_topdetail\">\n        <h2>"
    + escapeExpression(((helper = (helper = helpers.pagesaved || (depth0 != null ? depth0.pagesaved : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pagesaved","hash":{},"data":data}) : helper)))
    + "</h2>\n        <nav class=\"pkt_ext_item_actions pkt_ext_cf\">\n            <ul>\n                <li><a class=\"pkt_ext_removeitem\" href=\"#\">"
    + escapeExpression(((helper = (helper = helpers.removepage || (depth0 != null ? depth0.removepage : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"removepage","hash":{},"data":data}) : helper)))
    + "</a></li>\n                <li class=\"pkt_ext_actions_separator\"></li>                                \n                <li><a class=\"pkt_ext_openpocket\" href=\"http://firefox.dev.readitlater.com/a\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.viewlist || (depth0 != null ? depth0.viewlist : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"viewlist","hash":{},"data":data}) : helper)))
    + "</a></li>\n            </ul>\n        </nav>                        \n    </div>\n    <p class=\"pkt_ext_edit_msg\"></p>                        \n    <div class=\"pkt_ext_tag_detail pkt_ext_cf\">\n        <div class=\"pkt_ext_tag_input_wrapper\">\n            <div class=\"pkt_ext_tag_input_blocker\"></div>\n            <input class=\"pkt_ext_tag_input\" type=\"text\" placeholder=\""
    + escapeExpression(((helper = (helper = helpers.addtags || (depth0 != null ? depth0.addtags : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"addtags","hash":{},"data":data}) : helper)))
    + "\">\n        </div>\n        <a href=\"#\" class=\"pkt_ext_btn pkt_ext_btn_disabled\">"
    + escapeExpression(((helper = (helper = helpers.save || (depth0 != null ? depth0.save : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"save","hash":{},"data":data}) : helper)))
    + "</a>\n    </div>\n</div>";
},"useData":true});
templates['signup_shell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "<h4 class=\"pkt_ext_introrecommend\">Recommended by Firefox</h4>\n<div class=\"pkt_ext_introdetail\">\n	<h2 class=\"pkt_ext_logo\">Pocket</h2>\n	<div class=\"pkt_ext_introimg\"></div>\n	<p class=\"pkt_ext_tagline\">"
    + escapeExpression(((helper = (helper = helpers.tagline || (depth0 != null ? depth0.tagline : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"tagline","hash":{},"data":data}) : helper)))
    + "</p>\n	<p><a class=\"pkt_ext_learnmore\" href=\"http://getpocket.com\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n</div>\n<div class=\"pkt_ext_signupdetail\">\n	<h4>"
    + escapeExpression(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n	<p class=\"btn-container\"><a href=\"https://firefox.dev.readitlater.com/ff_signup\" target=_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + escapeExpression(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a> <a class=\"ff_signuphelp\" href=\"https://www.mozilla.org\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.help || (depth0 != null ? depth0.help : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"help","hash":{},"data":data}) : helper)))
    + "</a></p>\n	<p class=\"btn-container\"><a href=\"http://firefox.dev.readitlater.com/signup?force=email&src=extension\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\">"
    + escapeExpression(((helper = (helper = helpers.signupemail || (depth0 != null ? depth0.signupemail : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signupemail","hash":{},"data":data}) : helper)))
    + "</a></p>\n	<p class=\"alreadyhave\">"
    + escapeExpression(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"http://firefox.dev.readitlater.com/login?ep=3&src=extension\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n</div>";
},"useData":true});
})();