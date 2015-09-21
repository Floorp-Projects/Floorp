var {Ci} = require("chrome");
var Services = require("Services");

exports.setupParent = function ({mm, prefix}) {
  let args = [
    !!mm.QueryInterface(Ci.nsIMessageSender),
    prefix
  ];
  Services.obs.notifyObservers(null, "test:setupParent", JSON.stringify(args));
}
