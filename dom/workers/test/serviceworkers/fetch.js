function get_query_params(url) {
  var search = (new URL(url)).search;
  if (!search) {
    return {};
  }
  var ret = {};
  var params = search.substring(1).split('&');
  params.forEach(function(param) {
      var element = param.split('=');
      ret[decodeURIComponent(element[0])] = decodeURIComponent(element[1]);
  });
  return ret;
}

addEventListener('fetch', function(event) {
  if (event.request.url.includes('fail.html')) {
    event.respondWith(fetch('hello.html', { integrity: 'abc' }));
  } else if (event.request.url.includes('fake.html')) {
    event.respondWith(fetch('hello.html'));
  } else if (event.request.url.includes("file_js_cache")) {
    event.respondWith(fetch(event.request));
  } else if (event.request.url.includes('redirect')) {
    let param = get_query_params(event.request.url);
    let url = param['url'];
    let mode = param['mode'];

    event.respondWith(fetch(url, { mode: mode }));
  }
});

addEventListener('message', function(event) {
  if (event.data === 'claim') {
    event.waitUntil(clients.claim());
  }
});
