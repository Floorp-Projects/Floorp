setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 20);

setObjectMetadataCallback(true);
(function(){
  for(var i = 0; i < 100; i++) {
    try{
      var a = new Array(5);
      throw 1;
    } catch(e) {}
  }
})();
