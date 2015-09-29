// Binary: cache/js-dbg-32-fadb38356e0f-linux
// Flags: -j
//
load(libdir + "immutable-prototype.js");

if (globalPrototypeChainIsMutable())
{
  this.__proto__ = Proxy.create({
      has:function(){return false},
      set:function(){}
  });
}

(function(){
  eval("(function(){ for(var j=0;j<6;++j) if(j%2==1) p=0; })")();
})()
