var EXPORTED_SYMBOLS = ["target", "bound"];

var bound = "";
var target = {};
Services.scriptloader.loadSubScript("resource://test/environment_script.js", target);

// Check global bindings
try { void vu; bound += "vu,"; } catch (e) {}
try { void vq; bound += "vq,"; } catch (e) {}
try { void vl; bound += "vl,"; } catch (e) {}
try { void gt; bound += "gt,"; } catch (e) {}
try { void ed; bound += "ed,"; } catch (e) {}
try { void ei; bound += "ei,"; } catch (e) {}
try { void fo; bound += "fo,"; } catch (e) {}
try { void fi; bound += "fi,"; } catch (e) {}
try { void fd; bound += "fd,"; } catch (e) {}
