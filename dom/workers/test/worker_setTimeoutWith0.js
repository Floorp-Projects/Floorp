var x = 0;
setTimeout("x++; '\x00'; x++;");
setTimeout("postMessage(x);");
