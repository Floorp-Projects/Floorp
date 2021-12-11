function init() {
  function end() {
    document.documentElement.className = '';
  }

  function next() {
    compressAndDisplay(original, end);
  }

  var original = getImageFromDataUrl(sample);
  setImgLoadListener(original, next);
}

function compressAndDisplay(image, next) {
  var canvas = document.createElement('canvas');
  canvas.width = image.naturalWidth;
  canvas.height = image.naturalHeight;
  var ctx = canvas.getContext('2d');
  ctx.drawImage(image, 0, 0);

  function gotBlob(blob) {
    var img = getImageFromBlob(blob);
    setImgLoadListener(img, next);
    document.body.appendChild(img);
  }

  // I want to test passing 'undefined' as quality as well
  if ('quality' in window) {
    canvas.toBlob(gotBlob, 'image/jpeg', quality);
  } else {
    canvas.toBlob(gotBlob, 'image/jpeg');
  }
}

function setImgLoadListener(img, func) {
  if (img.complete) {
    func.call(img, { target: img});
  } else {
    img.addEventListener('load', func);
  }
}

function naturalDimensionsHandler(e) {
  var img = e.target;
  img.width = img.naturalWidth;
  img.height = img.naturalHeight;
}

function getImageFromBlob(blob) {
  var img = document.createElement('img');
  img.src = window.URL.createObjectURL(blob);
  setImgLoadListener(img, naturalDimensionsHandler);
  setImgLoadListener(img, function(e) {
    window.URL.revokeObjectURL(e.target.src);
  });

  return img;
}

function getImageFromDataUrl(url) {
  var img = document.createElement('img');
  img.src = url;
  setImgLoadListener(img, naturalDimensionsHandler);

  return img;
}

init();
