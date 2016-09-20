(function() {
  var template = Handlebars.template, templates = Handlebars.templates = Handlebars.templates || {};
templates['saved_premiumextras'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  return "<div class=\"pkt_ext_suggestedtag_detailshown\">\r\n</div> ";
  },"useData":true});
templates['saved_premiumshell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "<div class=\"pkt_ext_suggestedtag_detail pkt_ext_suggestedtag_detail_loading\">\n    <h4>"
    + escapeExpression(((helper = (helper = helpers.suggestedtags || (depth0 != null ? depth0.suggestedtags : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"suggestedtags","hash":{},"data":data}) : helper)))
    + "</h4>\n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n    <ul class=\"pkt_ext_cf\">\n    </ul>\n</div>";
},"useData":true});
templates['saved_shell'] = template({"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "<div class=\"pkt_ext_initload\">\n    <div class=\"pkt_ext_logo\"></div> \n    <div class=\"pkt_ext_topdetail\">\n        <h2>"
    + escapeExpression(((helper = (helper = helpers.saving || (depth0 != null ? depth0.saving : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"saving","hash":{},"data":data}) : helper)))
    + "</h2>\n    </div> \n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n</div>                                      \n<div class=\"pkt_ext_detail\">                        \n    <div class=\"pkt_ext_logo\"></div>\n    <div class=\"pkt_ext_topdetail\">\n        <h2>"
    + escapeExpression(((helper = (helper = helpers.pagesaved || (depth0 != null ? depth0.pagesaved : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pagesaved","hash":{},"data":data}) : helper)))
    + "</h2>\n        <h3 class=\"pkt_ext_errordetail\"></h3>\n        <nav class=\"pkt_ext_item_actions pkt_ext_cf\">\n            <ul>\n                <li><a class=\"pkt_ext_removeitem\" href=\"#\">"
    + escapeExpression(((helper = (helper = helpers.removepage || (depth0 != null ? depth0.removepage : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"removepage","hash":{},"data":data}) : helper)))
    + "</a></li>\n                <li class=\"pkt_ext_actions_separator\"></li>                                \n                <li><a class=\"pkt_ext_openpocket\" href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/a?src=ff_ext_saved\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.viewlist || (depth0 != null ? depth0.viewlist : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"viewlist","hash":{},"data":data}) : helper)))
    + "</a></li>\n            </ul>\n        </nav>                        \n    </div>\n    <div class=\"pkt_ext_tag_detail pkt_ext_cf\">\n        <div class=\"pkt_ext_tag_input_wrapper\">\n            <div class=\"pkt_ext_tag_input_blocker\"></div>\n            <input class=\"pkt_ext_tag_input\" type=\"text\" placeholder=\""
    + escapeExpression(((helper = (helper = helpers.addtags || (depth0 != null ? depth0.addtags : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"addtags","hash":{},"data":data}) : helper)))
    + "\">\n        </div>\n        <a href=\"#\" class=\"pkt_ext_btn pkt_ext_btn_disabled\">"
    + escapeExpression(((helper = (helper = helpers.save || (depth0 != null ? depth0.save : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"save","hash":{},"data":data}) : helper)))
    + "</a>\n    </div>\n    <p class=\"pkt_ext_edit_msg\"></p>\n</div>";
},"useData":true});
templates['signup_shell'] = template({"1":function(depth0,helpers,partials,data) {
  var stack1, buffer = "";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.controlvariant : depth0), {"name":"if","hash":{},"fn":this.program(2, data),"inverse":this.program(4, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  return buffer;
},"2":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "      <p class=\"pkt_ext_learnmorecontainer\"><a class=\"pkt_ext_learnmore\" href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_control&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"4":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "      <p class=\"pkt_ext_learnmorecontainer\"><a class=\"pkt_ext_learnmore\" href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_tryit&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"6":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "  <p class=\"pkt_ext_learnmorecontainer\"><a class=\"pkt_ext_learnmore pkt_ext_learnmoreinactive\" href=\"#\">"
    + escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"8":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "  <h4>"
    + escapeExpression(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n  <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&t=signupff&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + escapeExpression(((helper = (helper = helpers.signinfirefox || (depth0 != null ? depth0.signinfirefox : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signinfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n  <p class=\"alreadyhave\">"
    + escapeExpression(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&src=extension&s=ffi&t=login&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"10":function(depth0,helpers,partials,data) {
  var stack1, buffer = "";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.controlvariant : depth0), {"name":"if","hash":{},"fn":this.program(11, data),"inverse":this.program(13, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  return buffer;
},"11":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "      <h4>"
    + escapeExpression(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n      <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&tv=panel_control&t=signupff&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + escapeExpression(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n      <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/signup?force=email&tv=panel_control&src=extension&s=ffi&t=signupemail&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\">"
    + escapeExpression(((helper = (helper = helpers.signupemail || (depth0 != null ? depth0.signupemail : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signupemail","hash":{},"data":data}) : helper)))
    + "</a></p>\n     <p class=\"alreadyhave\">"
    + escapeExpression(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&tv=panel_control&src=extension&s=ffi&t=login&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"13":function(depth0,helpers,partials,data) {
  var stack1, helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression, buffer = "      <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_tryitnow?s=ffi&tv=panel_tryit&t=tryitnow\" target=\"_blank\" class=\"btn signup-btn-tryitnow\"><span class=\"text\">"
    + escapeExpression(((helper = (helper = helpers.tryitnow || (depth0 != null ? depth0.tryitnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"tryitnow","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n      <p class=\"alreadyhave tryitnowspace\">"
    + escapeExpression(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&s=ffi&tv=panel_tryit&src=extension&t=login&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n      <p class=\"pkt_ext_tos\">";
  stack1 = ((helper = (helper = helpers.tos || (depth0 != null ? depth0.tos : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"tos","hash":{},"data":data}) : helper));
  if (stack1 != null) { buffer += stack1; }
  return buffer + "</p>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  var stack1, helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression, buffer = "<div class=\"pkt_ext_introdetail pkt_ext_introdetailhero\">\n <h2 class=\"pkt_ext_logo\">Pocket</h2>\n  <p class=\"pkt_ext_tagline\">"
    + escapeExpression(((helper = (helper = helpers.tagline || (depth0 != null ? depth0.tagline : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"tagline","hash":{},"data":data}) : helper)))
    + "</p>\n";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.showlearnmore : depth0), {"name":"if","hash":{},"fn":this.program(1, data),"inverse":this.program(6, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  buffer += " <div class=\"pkt_ext_introimg\"></div>\n</div>\n<div class=\"pkt_ext_signupdetail pkt_ext_signupdetail_hero\">\n";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.fxasignedin : depth0), {"name":"if","hash":{},"fn":this.program(8, data),"inverse":this.program(10, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  return buffer + "</div>\n";
},"useData":true});
templates['signupstoryboard_shell'] = template({"1":function(depth0,helpers,partials,data) {
  var stack1, buffer = "";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.controlvariant : depth0), {"name":"if","hash":{},"fn":this.program(2, data),"inverse":this.program(4, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  return buffer;
},"2":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "          <p><a class=\"pkt_ext_learnmore\" href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_control&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"4":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "          <p><a class=\"pkt_ext_learnmore\" href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_learnmore?s=ffi&t=learnmore&tv=panel_tryit&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"6":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "      <p><a class=\"pkt_ext_learnmore pkt_ext_learnmoreinactive\" href=\"#\">"
    + escapeExpression(((helper = (helper = helpers.learnmore || (depth0 != null ? depth0.learnmore : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"learnmore","hash":{},"data":data}) : helper)))
    + "</a></p>\n";
},"8":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "  <h4>"
    + escapeExpression(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n  <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&t=signupff&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + escapeExpression(((helper = (helper = helpers.signinfirefox || (depth0 != null ? depth0.signinfirefox : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signinfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n  <p class=\"alreadyhave\">"
    + escapeExpression(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&src=extension&s=ffi&t=login&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"10":function(depth0,helpers,partials,data) {
  var stack1, buffer = "";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.controlvariant : depth0), {"name":"if","hash":{},"fn":this.program(11, data),"inverse":this.program(13, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  return buffer;
},"11":function(depth0,helpers,partials,data) {
  var helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression;
  return "      <h4>"
    + escapeExpression(((helper = (helper = helpers.signuptosave || (depth0 != null ? depth0.signuptosave : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signuptosave","hash":{},"data":data}) : helper)))
    + "</h4>\n      <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/ff_signup?s=ffi&tv=panel_control&t=signupff&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\">"
    + escapeExpression(((helper = (helper = helpers.signupfirefox || (depth0 != null ? depth0.signupfirefox : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signupfirefox","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n      <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/signup?force=email&tv=panel_control&src=extension&s=ffi&t=signupemail&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\">"
    + escapeExpression(((helper = (helper = helpers.signupemail || (depth0 != null ? depth0.signupemail : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"signupemail","hash":{},"data":data}) : helper)))
    + "</a></p>\n     <p class=\"alreadyhave\">"
    + escapeExpression(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&tv=panel_control&src=extension&s=ffi&t=login&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n";
},"13":function(depth0,helpers,partials,data) {
  var stack1, helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression, buffer = "      <p class=\"btn-container\"><a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/firefox_tryitnow?s=ffi&tv=panel_tryit&t=tryitnow\" target=\"_blank\" class=\"btn signup-btn-tryitnow\"><span class=\"text\">"
    + escapeExpression(((helper = (helper = helpers.tryitnow || (depth0 != null ? depth0.tryitnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"tryitnow","hash":{},"data":data}) : helper)))
    + "</span></a></p>\n      <p class=\"alreadyhave tryitnowspace\">"
    + escapeExpression(((helper = (helper = helpers.alreadyhaveacct || (depth0 != null ? depth0.alreadyhaveacct : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"alreadyhaveacct","hash":{},"data":data}) : helper)))
    + " <a href=\"https://"
    + escapeExpression(((helper = (helper = helpers.pockethost || (depth0 != null ? depth0.pockethost : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"pockethost","hash":{},"data":data}) : helper)))
    + "/login?ep=3&s=ffi&tv=panel_tryit&src=extension&t=login&v="
    + escapeExpression(((helper = (helper = helpers.variant || (depth0 != null ? depth0.variant : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"variant","hash":{},"data":data}) : helper)))
    + "\" target=\"_blank\">"
    + escapeExpression(((helper = (helper = helpers.loginnow || (depth0 != null ? depth0.loginnow : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"loginnow","hash":{},"data":data}) : helper)))
    + "</a>.</p>\n      <p class=\"pkt_ext_tos\">";
  stack1 = ((helper = (helper = helpers.tos || (depth0 != null ? depth0.tos : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"tos","hash":{},"data":data}) : helper));
  if (stack1 != null) { buffer += stack1; }
  return buffer + "</p>\n";
},"compiler":[6,">= 2.0.0-beta.1"],"main":function(depth0,helpers,partials,data) {
  var stack1, helper, functionType="function", helperMissing=helpers.helperMissing, escapeExpression=this.escapeExpression, buffer = "<div class=\"pkt_ext_introdetail pkt_ext_introdetailstoryboard\">\n <div class=\"pkt_ext_introstory pkt_ext_introstoryone\">\n    <div class=\"pkt_ext_introstory_text\">\n     <p class=\"pkt_ext_tagline\">"
    + escapeExpression(((helper = (helper = helpers.taglinestory_one || (depth0 != null ? depth0.taglinestory_one : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"taglinestory_one","hash":{},"data":data}) : helper)))
    + "</p>\n   </div>\n    <div class=\"pkt_ext_introstoryone_img\"></div>\n </div>\n  <div class=\"pkt_ext_introstorydivider\"></div>\n <div class=\"pkt_ext_introstory pkt_ext_introstorytwo\">\n    <div class=\"pkt_ext_introstory_text\">\n     <p class=\"pkt_ext_tagline\">"
    + escapeExpression(((helper = (helper = helpers.taglinestory_two || (depth0 != null ? depth0.taglinestory_two : depth0)) != null ? helper : helperMissing),(typeof helper === functionType ? helper.call(depth0, {"name":"taglinestory_two","hash":{},"data":data}) : helper)))
    + "</p>\n";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.showlearnmore : depth0), {"name":"if","hash":{},"fn":this.program(1, data),"inverse":this.program(6, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  buffer += "   </div>\n    <div class=\"pkt_ext_introstorytwo_img\"></div>\n </div>\n</div>\n<div class=\"pkt_ext_signupdetail\">\n";
  stack1 = helpers['if'].call(depth0, (depth0 != null ? depth0.fxasignedin : depth0), {"name":"if","hash":{},"fn":this.program(8, data),"inverse":this.program(10, data),"data":data});
  if (stack1 != null) { buffer += stack1; }
  return buffer + "\n</div>\n";
},"useData":true});
})();
