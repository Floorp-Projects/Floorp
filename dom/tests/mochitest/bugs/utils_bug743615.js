function makePattern(len, start, inc) {
  var pattern = [];
  while(len) {
    pattern.push(start);
    start = (start + inc) % 256;
    --len;
  }
  return pattern;
}

function setPattern(imageData, pattern) {
  if (pattern.length != imageData.data.length)
    throw Error('Invalid pattern');
  for (var i = 0; i < pattern.length; ++i)
    imageData.data[i] = pattern[i];
}

function checkPattern(imageData, pattern) {
  if (pattern.length != imageData.data.length)
    throw Error('Invalid pattern');
  for (var i = 0; i < pattern.length; ++i)
    if (imageData.data[i] != pattern[i])
      return false;
  return true;
}
