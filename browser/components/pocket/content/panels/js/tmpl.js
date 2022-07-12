(function() {
  var template = Handlebars.template, templates = Handlebars.templates = Handlebars.templates || {};
templates['saved_premiumshell'] = template({"compiler":[8,">= 4.3.0"],"main":function(container,depth0,helpers,partials,data) {
    return "<div class=\"pkt_ext_suggestedtag_detail pkt_ext_suggestedtag_detail_loading\">\n    <h4 data-l10n-id=\"pocket-panel-saved-suggested-tags\"></h4>\n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n    <ul class=\"pkt_ext_cf\">\n    </ul>\n</div>\n\n<hr/>\n";
},"useData":true});
templates['saved_shell'] = template({"compiler":[8,">= 4.3.0"],"main":function(container,depth0,helpers,partials,data) {
    var helper, lookupProperty = container.lookupProperty || function(parent, propertyName) {
        if (Object.prototype.hasOwnProperty.call(parent, propertyName)) {
          return parent[propertyName];
        }
        return undefined
    };

  return "<div class=\"pkt_ext_initload\">\n    <div class=\"pkt_ext_logo\"></div>\n    <div class=\"pkt_ext_topdetail\">\n        <h2 data-l10n-id=\"pocket-panel-saved-saving-tags\"></h2>\n    </div>\n    <div class=\"pkt_ext_loadingspinner\"><div></div></div>\n</div>\n<div class=\"pkt_ext_detail\">\n    <div class=\"pkt_ext_logo\"></div>\n    <div class=\"pkt_ext_topdetail\">\n        <h2></h2>\n        <h3 class=\"pkt_ext_errordetail\"></h3>\n        <nav class=\"pkt_ext_item_actions pkt_ext_cf\">\n            <ul>\n                <li><a class=\"pkt_ext_removeitem\" href=\"#\" data-l10n-id=\"pocket-panel-saved-remove-page\"></a></li>\n                <li class=\"pkt_ext_actions_separator\"></li>\n                <li><a class=\"pkt_ext_openpocket\" href=\"https://"
    + container.escapeExpression(((helper = (helper = lookupProperty(helpers,"pockethost") || (depth0 != null ? lookupProperty(depth0,"pockethost") : depth0)) != null ? helper : container.hooks.helperMissing),(typeof helper === "function" ? helper.call(depth0 != null ? depth0 : (container.nullContext || {}),{"name":"pockethost","hash":{},"data":data,"loc":{"start":{"line":17,"column":64},"end":{"line":17,"column":78}}}) : helper)))
    + "/a?src=ff_ext_saved\" target=\"_blank\" data-l10n-id=\"pocket-panel-signup-view-list\"></a></li>\n            </ul>\n        </nav>\n    </div>\n    <div class=\"pkt_ext_tag_detail pkt_ext_cf\">\n        <div class=\"pkt_ext_tag_input_wrapper\">\n            <div class=\"pkt_ext_tag_input_blocker\"></div>\n            <input class=\"pkt_ext_tag_input\" type=\"text\" data-l10n-id=\"pocket-panel-saved-add-tags\">\n        </div>\n        <a href=\"#\" class=\"pkt_ext_btn pkt_ext_btn_disabled\" data-l10n-id=\"pocket-panel-saved-save-tags\"></a>\n    </div>\n    <div class=\"pkt_ext_edit_msg_container\">\n      <p class=\"pkt_ext_edit_msg\"></p>\n    </div>\n</div>\n\n<div class=\"pkt_ext_subshell\">\n    <div class=\"pkt_ext_item_recs\"></div>\n</div>\n";
},"useData":true});
templates['signup_shell'] = template({"compiler":[8,">= 4.3.0"],"main":function(container,depth0,helpers,partials,data) {
    var helper, alias1=depth0 != null ? depth0 : (container.nullContext || {}), alias2=container.hooks.helperMissing, alias3="function", alias4=container.escapeExpression, lookupProperty = container.lookupProperty || function(parent, propertyName) {
        if (Object.prototype.hasOwnProperty.call(parent, propertyName)) {
          return parent[propertyName];
        }
        return undefined
    };

  return "<div class=\"pkt_ext_introdetail pkt_ext_introdetailstoryboard\">\n	<div class=\"pkt_ext_introstory pkt_ext_introstoryone\">\n		<div class=\"pkt_ext_introstory_text\">\n			<p class=\"pkt_ext_tagline\" data-l10n-id=\"pocket-panel-signup-tagline-story-one\"></p>\n		</div>\n		<div class=\"pkt_ext_introstoryone_img\"></div>\n	</div>\n	<div class=\"pkt_ext_introstorydivider\"></div>\n	<div class=\"pkt_ext_introstory pkt_ext_introstorytwo\">\n		<div class=\"pkt_ext_introstory_text\">\n      <p class=\"pkt_ext_tagline\" data-l10n-id=\"pocket-panel-signup-tagline-story-two\"></p>\n			<p><a class=\"pkt_ext_learnmore\" href=\"https://"
    + alias4(((helper = (helper = lookupProperty(helpers,"pockethost") || (depth0 != null ? lookupProperty(depth0,"pockethost") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"pockethost","hash":{},"data":data,"loc":{"start":{"line":12,"column":49},"end":{"line":12,"column":63}}}) : helper)))
    + "/firefox_learnmore?utm_campaign="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmCampaign") || (depth0 != null ? lookupProperty(depth0,"utmCampaign") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmCampaign","hash":{},"data":data,"loc":{"start":{"line":12,"column":95},"end":{"line":12,"column":110}}}) : helper)))
    + "&utm_source="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmSource") || (depth0 != null ? lookupProperty(depth0,"utmSource") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmSource","hash":{},"data":data,"loc":{"start":{"line":12,"column":122},"end":{"line":12,"column":135}}}) : helper)))
    + "&s=ffi\" target=\"_blank\" data-l10n-id=\"pocket-panel-signup-learn-more\"></a></p>\n		</div>\n		<div class=\"pkt_ext_introstorytwo_img\"></div>\n	</div>\n</div>\n<div class=\"pkt_ext_signupdetail\">\n	<h4 data-l10n-id=\"pocket-panel-signup-signup-cta\"></h4>\n  <p class=\"btn-container btn-container-firefox\"><a href=\"https://"
    + alias4(((helper = (helper = lookupProperty(helpers,"pockethost") || (depth0 != null ? lookupProperty(depth0,"pockethost") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"pockethost","hash":{},"data":data,"loc":{"start":{"line":19,"column":66},"end":{"line":19,"column":80}}}) : helper)))
    + "/ff_signup?utm_campaign="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmCampaign") || (depth0 != null ? lookupProperty(depth0,"utmCampaign") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmCampaign","hash":{},"data":data,"loc":{"start":{"line":19,"column":104},"end":{"line":19,"column":119}}}) : helper)))
    + "&utm_source="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmSource") || (depth0 != null ? lookupProperty(depth0,"utmSource") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmSource","hash":{},"data":data,"loc":{"start":{"line":19,"column":131},"end":{"line":19,"column":144}}}) : helper)))
    + "&s=ffi\" target=\"_blank\" class=\"btn signup-btn-firefox\"><span class=\"logo\"></span><span class=\"text\" data-l10n-id=\"pocket-panel-signup-signup-firefox\"></span></a></p>\n  <p class=\"btn-container btn-container-email\"><a href=\"https://"
    + alias4(((helper = (helper = lookupProperty(helpers,"pockethost") || (depth0 != null ? lookupProperty(depth0,"pockethost") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"pockethost","hash":{},"data":data,"loc":{"start":{"line":20,"column":64},"end":{"line":20,"column":78}}}) : helper)))
    + "/signup?utm_campaign="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmCampaign") || (depth0 != null ? lookupProperty(depth0,"utmCampaign") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmCampaign","hash":{},"data":data,"loc":{"start":{"line":20,"column":99},"end":{"line":20,"column":114}}}) : helper)))
    + "&utm_source="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmSource") || (depth0 != null ? lookupProperty(depth0,"utmSource") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmSource","hash":{},"data":data,"loc":{"start":{"line":20,"column":126},"end":{"line":20,"column":139}}}) : helper)))
    + "&force=email&src=extension&s=ffi\" target=\"_blank\" class=\"btn btn-secondary signup-btn-email signup-btn-initstate\" data-l10n-id=\"pocket-panel-signup-signup-email\"></a></p>\n	<p class=\"alreadyhave\"><span data-l10n-id=\"pocket-panel-signup-already-have\"></span> <a class=\"pkt_ext_login\" href=\"https://"
    + alias4(((helper = (helper = lookupProperty(helpers,"pockethost") || (depth0 != null ? lookupProperty(depth0,"pockethost") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"pockethost","hash":{},"data":data,"loc":{"start":{"line":21,"column":125},"end":{"line":21,"column":139}}}) : helper)))
    + "/login?utm_campaign="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmCampaign") || (depth0 != null ? lookupProperty(depth0,"utmCampaign") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmCampaign","hash":{},"data":data,"loc":{"start":{"line":21,"column":159},"end":{"line":21,"column":174}}}) : helper)))
    + "&utm_source="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmSource") || (depth0 != null ? lookupProperty(depth0,"utmSource") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmSource","hash":{},"data":data,"loc":{"start":{"line":21,"column":186},"end":{"line":21,"column":199}}}) : helper)))
    + "&src=extension&s=ffi\" target=\"_blank\" data-l10n-id=\"pocket-panel-signup-login\"></a>.</p>\n</div>\n";
},"useData":true});
templates['home_shell'] = template({"compiler":[8,">= 4.3.0"],"main":function(container,depth0,helpers,partials,data) {
    var helper, lookupProperty = container.lookupProperty || function(parent, propertyName) {
        if (Object.prototype.hasOwnProperty.call(parent, propertyName)) {
          return parent[propertyName];
        }
        return undefined
    };

  return "<div class=\"pkt_ext_wrapperhome\">\n  <div class=\"pkt_ext_home\">\n    <div class=\"pkt_ext_header\">\n      <div class=\"pkt_ext_logo\"></div>\n      <a class=\"pkt_ext_mylist\" href=\"https://"
    + container.escapeExpression(((helper = (helper = lookupProperty(helpers,"pockethost") || (depth0 != null ? lookupProperty(depth0,"pockethost") : depth0)) != null ? helper : container.hooks.helperMissing),(typeof helper === "function" ? helper.call(depth0 != null ? depth0 : (container.nullContext || {}),{"name":"pockethost","hash":{},"data":data,"loc":{"start":{"line":5,"column":46},"end":{"line":5,"column":60}}}) : helper)))
    + "/a?src=ff_ext_home\" target=\"_blank\">\n        <span class=\"pkt_ext_mylist_icon\"></span>\n        <span data-l10n-id=\"pocket-panel-home-my-list\"></span>\n      </a>\n    </div>\n    <div class=\"pkt_ext_hr\"></div>\n    <div class=\"pkt_ext_detail\">\n      <h2 data-l10n-id=\"pocket-panel-home-welcome-back\"></h2>\n      <p data-l10n-id=\"pocket-panel-home-paragraph\"></p>\n      <div class=\"pkt_ext_more\"></div>\n    </div>\n  </div>\n</div>\n";
},"useData":true});
templates['explore_more'] = template({"compiler":[8,">= 4.3.0"],"main":function(container,depth0,helpers,partials,data) {
    var helper, alias1=depth0 != null ? depth0 : (container.nullContext || {}), alias2=container.hooks.helperMissing, alias3="function", alias4=container.escapeExpression, lookupProperty = container.lookupProperty || function(parent, propertyName) {
        if (Object.prototype.hasOwnProperty.call(parent, propertyName)) {
          return parent[propertyName];
        }
        return undefined
    };

  return "<a class=\"pkt_ext_discover\" href=\"https://"
    + alias4(((helper = (helper = lookupProperty(helpers,"pockethost") || (depth0 != null ? lookupProperty(depth0,"pockethost") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"pockethost","hash":{},"data":data,"loc":{"start":{"line":1,"column":42},"end":{"line":1,"column":56}}}) : helper)))
    + "/explore?utm_source="
    + alias4(((helper = (helper = lookupProperty(helpers,"utmsource") || (depth0 != null ? lookupProperty(depth0,"utmsource") : depth0)) != null ? helper : alias2),(typeof helper === alias3 ? helper.call(alias1,{"name":"utmsource","hash":{},"data":data,"loc":{"start":{"line":1,"column":76},"end":{"line":1,"column":89}}}) : helper)))
    + "\" data-l10n-id=\"pocket-panel-home-explore-more\"></a>\n";
},"useData":true});
templates['item_recs'] = template({"1":function(container,depth0,helpers,partials,data) {
    return "  <h4>Similar Stories</h4>\n";
},"3":function(container,depth0,helpers,partials,data) {
    return "  <h4>Similar Story</h4>\n";
},"5":function(container,depth0,helpers,partials,data) {
    var stack1, alias1=container.lambda, alias2=container.escapeExpression, lookupProperty = container.lookupProperty || function(parent, propertyName) {
        if (Object.prototype.hasOwnProperty.call(parent, propertyName)) {
          return parent[propertyName];
        }
        return undefined
    };

  return "    <li>\n      <a href=\""
    + alias2(alias1(((stack1 = (depth0 != null ? lookupProperty(depth0,"item") : depth0)) != null ? lookupProperty(stack1,"resolved_url") : stack1), depth0))
    + "\" class=\"pkt_ext_item_recs_link\" target=\"_blank\">\n\n"
    + ((stack1 = lookupProperty(helpers,"if").call(depth0 != null ? depth0 : (container.nullContext || {}),((stack1 = (depth0 != null ? lookupProperty(depth0,"item") : depth0)) != null ? lookupProperty(stack1,"encodedThumbURL") : stack1),{"name":"if","hash":{},"fn":container.program(6, data, 0),"inverse":container.noop,"data":data,"loc":{"start":{"line":15,"column":8},"end":{"line":17,"column":15}}})) != null ? stack1 : "")
    + "\n        <p class=\"rec-title\">"
    + alias2(alias1(((stack1 = (depth0 != null ? lookupProperty(depth0,"item") : depth0)) != null ? lookupProperty(stack1,"title") : stack1), depth0))
    + "</p>\n        <p class=\"rec-source\">"
    + alias2(alias1(((stack1 = ((stack1 = (depth0 != null ? lookupProperty(depth0,"item") : depth0)) != null ? lookupProperty(stack1,"domain_metadata") : stack1)) != null ? lookupProperty(stack1,"name") : stack1), depth0))
    + "</p>\n      </a>\n    </li>\n";
},"6":function(container,depth0,helpers,partials,data) {
    var stack1, lookupProperty = container.lookupProperty || function(parent, propertyName) {
        if (Object.prototype.hasOwnProperty.call(parent, propertyName)) {
          return parent[propertyName];
        }
        return undefined
    };

  return "        <img class=\"rec-thumb\" src=\"https://img-getpocket.cdn.mozilla.net/80x80/filters:format(jpeg):quality(60):no_upscale():strip_exif()/"
    + container.escapeExpression(container.lambda(((stack1 = (depth0 != null ? lookupProperty(depth0,"item") : depth0)) != null ? lookupProperty(stack1,"encodedThumbURL") : stack1), depth0))
    + "\" />\n";
},"compiler":[8,">= 4.3.0"],"main":function(container,depth0,helpers,partials,data) {
    var stack1, alias1=depth0 != null ? depth0 : (container.nullContext || {}), lookupProperty = container.lookupProperty || function(parent, propertyName) {
        if (Object.prototype.hasOwnProperty.call(parent, propertyName)) {
          return parent[propertyName];
        }
        return undefined
    };

  return "<header>\n"
    + ((stack1 = lookupProperty(helpers,"if").call(alias1,((stack1 = (depth0 != null ? lookupProperty(depth0,"recommendations") : depth0)) != null ? lookupProperty(stack1,"1") : stack1),{"name":"if","hash":{},"fn":container.program(1, data, 0),"inverse":container.program(3, data, 0),"data":data,"loc":{"start":{"line":2,"column":2},"end":{"line":6,"column":9}}})) != null ? stack1 : "")
    + "  <a class=\"pkt_ext_learn_more\" target=\"_blank\" href=\"https://getpocket.com/story_recommendations_learn_more\">Learn more</a>\n</header>\n\n<ol>\n"
    + ((stack1 = lookupProperty(helpers,"each").call(alias1,(depth0 != null ? lookupProperty(depth0,"recommendations") : depth0),{"name":"each","hash":{},"fn":container.program(5, data, 0),"inverse":container.noop,"data":data,"loc":{"start":{"line":11,"column":2},"end":{"line":23,"column":11}}})) != null ? stack1 : "")
    + "</ol>\n";
},"useData":true});
})();