"use strict";

exports.setupParent = function({ mm, prefix }) {
  const args = [ChromeUtils.getClassName(mm) == "ChromeMessageSender", prefix];
  Services.obs.notifyObservers(null, "test:setupParent", JSON.stringify(args));
  return {};
};
