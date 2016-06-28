/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var xhr = new XMLHttpRequest();
xhr.open("GET", "worker_testXHR.txt");
xhr.send(null);
xhr.open("GET", "worker_testXHR.txt");
xhr.send(null);
postMessage("done");
