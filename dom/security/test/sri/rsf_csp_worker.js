postMessage("good_worker_could_load");
try {
  importScripts('rsf_imported.js');
} catch(e) {
  postMessage("good_worker_after_importscripts");
}
finally {
  postMessage("finish");
}
