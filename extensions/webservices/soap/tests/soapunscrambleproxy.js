function Unscramble()
{
  this.PROXY = new SOAPProxy(this, 0, 
    "http://ray.dsl.xmission.com:8080/soap/servlet/rpcrouter", 
    "http://soaptests.mozilla.org/unscramble", 
    true);                  // true = friendly to untrusted applets
}

//  Object to proxy methods

Unscramble.prototype =
{
  //  Standard items on all SOAP proxies using SOAPProxy.
  PROXY: null,
  //  Proxy-specific items
  unscramble: function(language, word, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    type = this.PROXY.getType("string");
    return this.PROXY.invoke(
      "unscramble",
      null, new Array(
      new SOAPParameter(language,"language",null,type),
      new SOAPParameter(word,"word",null,type)),
      function(proxy, headers, parameters)  //  This interprets the response
      {
        proxy.PROXY.result = null;
        if (parameters != null) {
          var parameter = parameters[0];
          //  The following line can be used to force a result type
          //  parameter.schemaType = proxy.PROXY.getType("boolean");
          proxy.PROXY.result = parameter.value;
        }
        if (proxy.PROXY.oncompletion != null) {
          proxy.PROXY.oncompletion(proxy.PROXY.result);
        }
      }, oncompletion);
  },
  languages: function(oncompletion)
  {
    var type = null;
    return this.PROXY.invoke("languages",null,null,
      function(proxy, headers, parameters)  //  This interprets the response
      {
        proxy.PROXY.result = null;
        if (parameters != null) {
          var parameter = parameters[0];
          //  The following line can be used to force a result type
          //  parameter.schemaType = proxy.PROXY.getType("boolean");
          proxy.PROXY.result = parameter.value;
        }
        if (proxy.PROXY.oncompletion != null) {
          proxy.PROXY.oncompletion(proxy.PROXY.result);
        }
      }, oncompletion);
  }
}
