function IsPrime()
{
  this.PROXY = new SOAPProxy(this, 0,
    "http://ray.dsl.xmission.com:8080/soap/servlet/rpcrouter", 
    "http://soaptests.mozilla.org/isprime", 
    true);                  // true = friendly to untrusted applets
}

//  Object to proxy methods

IsPrime.prototype =
{
  //  Proxy-specific items
  isPrime: function(number, oncompletion)
  {
    var type = null;
    //  The following line can be used to force a parameter type
    //  type = this.PROXY.getType("long");
    return this.PROXY.invoke(
      "isPrime",
      null, 
      new Array(new SOAPParameter(number,"number",null,type)),
      function(proxy, headers, parameters)
      {
        proxy.PROXY.result = null;
        if (parameters != null) {
          var parameter = parameters[0];
          //  The following line can be used to force a result type
          parameter.schemaType = proxy.PROXY.getType("boolean");
          proxy.PROXY.result = parameter.value;
        }
        if (proxy.PROXY.oncompletion != null) {
          proxy.PROXY.oncompletion(proxy.PROXY.result);
        }
      }, oncompletion);
  }
}
