// -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-
// vim: set ts=2 sw=2 et tw=78:
function clearSelection(w)
{
  var sel = (w ? w : window).getSelection();
  sel.removeAllRanges();
}

function getNode(e, index) { 
  if (!(typeof index==='number')) return index;
  if (index >= 0) return e.childNodes[index];
  while (++index != 0) e = e.parentNode;
  return e;
}

function dragSelectPointsWithData()
{
  var event = arguments[0];
  var e = arguments[1];
  var x1 = arguments[2];
  var y1 = arguments[3];
  var x2 = arguments[4];
  var y2 = arguments[5];
  dir = x2 > x1 ? 1 : -1;
  event['type'] = "mousedown";
  synthesizeMouse(e, x1, y1, event);
  event['type'] = "mousemove";
  synthesizeMouse(e, x1 + dir, y1, event);
  for (var i = 6; i < arguments.length; ++i)
    synthesizeMouse(e, arguments[i], y1, event);
  synthesizeMouse(e, x2 - dir, y2, event);
  event['type'] = "mouseup";
  synthesizeMouse(e, x2, y2, event);
}

function dragSelectPoints()
{
  var args = Array.prototype.slice.call(arguments);
  dragSelectPointsWithData.apply(this, [{}].concat(args));
}

function dragSelect()
{
  var args = Array.prototype.slice.call(arguments);
  var e = args.shift();
  var x1 = args.shift();
  var x2 = args.shift();
  dragSelectPointsWithData.apply(this, [{},e,x1,5,x2,5].concat(args));
}

function accelDragSelect()
{
  var args = Array.prototype.slice.call(arguments);
  var e = args.shift();
  var x1 = args.shift();
  var x2 = args.shift();
  var y = args.length != 0 ? args.shift() : 5;
  dragSelectPointsWithData.apply(this, [{accelKey: true},e,x1,y,x2,y].concat(args));
}

function shiftClick(e, x, y)
{
  function pos(p) { return (typeof p === "undefined") ? 5 : p }
  synthesizeMouse(e, pos(x), pos(y), { shiftKey: true });
}

function keyRepeat(key,data,repeat)
{
  while (repeat-- > 0)
    synthesizeKey(key, data);
}

function keyRight(data)
{
  var repeat = arguments.length > 1 ? arguments[1] : 1;
  keyRepeat("VK_RIGHT", data, repeat);
}

function keyLeft(data)
{
  var repeat = arguments.length > 1 ? arguments[1] : 1;
  keyRepeat("VK_LEFT", data, repeat);
}

function shiftAccelClick(e, x, y)
{
  function pos(p) { return (typeof p === "undefined") ? 5 : p }
  synthesizeMouse(e, pos(x), pos(y), { shiftKey: true, accelKey: true });
}

function accelClick(e, x, y)
{
  function pos(p) { return (typeof p === "undefined") ? 5 : p }
  synthesizeMouse(e, pos(x), pos(y), { accelKey: true });
}

function addChildRanges(arr, e)
{
  var sel = window.getSelection();
  for (i = 0; i < arr.length; ++i) {
    var data = arr[i];
    var r = new Range()
    r.setStart(getNode(e, data[0]), data[1]);
    r.setEnd(getNode(e, data[2]), data[3]);
    sel.addRange(r);
  }
}

function checkText(text, e)
{
  var sel = window.getSelection();
  is(sel.toString(), text, e.id + ": selected text")
}

function checkRangeText(text, index)
{
  var r = window.getSelection().getRangeAt(index);
  is(r.toString(), text, e.id + ": range["+index+"].toString()")
}

function checkRangeCount(n, e)
{
  var sel = window.getSelection();
  is(sel.rangeCount, n, e.id + ": Selection range count");
}

function checkRangePoints(i, expected, e) {
  var sel = window.getSelection();
  if (i >= sel.rangeCount) return;
  var r = sel.getRangeAt(i);
  is(r.startContainer, expected[0], e.id + ": range.startContainer");
  is(r.startOffset, expected[1], e.id + ": range.startOffset");
  is(r.endContainer, expected[2], e.id + ": range.endContainer");
  is(r.endOffset, expected[3], e.id + ": range.endOffset");
}

function checkRange(i, expected, e) {
  var sel = window.getSelection();
  if (i >= sel.rangeCount) return;
  var r = sel.getRangeAt(i);
  is(r.startContainer, getNode(e, expected[0]), e.id + ": range["+i+"].startContainer");
  is(r.startOffset, expected[1], e.id + ": range["+i+"].startOffset");
  is(r.endContainer, getNode(e, expected[2]), e.id + ": range["+i+"].endContainer");
  is(r.endOffset, expected[3], e.id + ": range["+i+"].endOffset");
}

function checkRanges(arr, e)
{
  checkRangeCount(arr.length, e);
  for (i = 0; i < arr.length; ++i) {
    var expected = arr[i];
    checkRange(i, expected, e);
  }
}
