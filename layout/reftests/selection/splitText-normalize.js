/** Test for Bug 191864 **/
var tests_done = 0;
var tests = [
 [ {}, [0,4], "012345678" ],
 [ {}, [0,0], "012345678" ],
 [ {}, [0,9], "012345678" ],
 [ {startOffset:4}, [0,4], "012345678" ],
 [ {startOffset:5}, [0,4], "012345678" ],
 [ {startOffset:5,endOffset:6}, [0,4], "012345678" ],
 [ {endOffset:5}, [0,4], "012345678" ],
 [ {endOffset:4}, [0,4], "012345678" ],
 [ {endOffset:3}, [0,4], "012345678" ],
 [ {startOffset:1,endOffset:3}, [0,4], "012345678" ],
 [ {startOffset:7,endOffset:7}, [0,4], "012345678" ],
 [ {startOffset:4,endOffset:4}, [0,4], "012345678" ],
 [ {endNode:1}, [0,4], "012345678", "" ],
 [ {endNode:1}, [0,4], "01234567", "8" ],
 [ {endNode:1}, [1,4], "0", "12345678" ],
 [ {startOffset:1,endNode:1}, [0,0], "0", "12345678" ],
 [ {endNode:2}, [1,4], "0", "12345", "678" ],
]

function runtest(f,t,nosplit) {
  // create content
  let doc = f.contentDocument;
  for (let i = 2; i < t.length; ++i) {
    c = doc.createTextNode(t[i]);
    doc.body.appendChild(c);
  }

  // setup selection
  let sel = t[0]
  let startNode = sel.startNode === undefined ? doc.body.firstChild : doc.body.childNodes[sel.startNode];
  let startOffset = sel.startOffset === undefined ? 0 : sel.startOffset;
  let endNode = sel.endNode === undefined ? startNode : doc.body.childNodes[sel.endNode];
  let endOffset = sel.endOffset === undefined ? endNode.length : sel.endOffset;
  let selection = f.contentWindow.getSelection();
  let r = doc.createRange();
  r.setStart(startNode, startOffset);
  r.setEnd(endNode, endOffset);
  selection.addRange(r);

  // splitText
  let split = t[1]
  if (!nosplit)
    doc.body.childNodes[split[0]].splitText(split[1])
}
function test_ref(f,t,nosplit) {
  runtest(f,t,true);
}
function test_split(f,t) {
  runtest(f,t);
}
function test_split_insert(f,t) {
  runtest(f,t);
  let doc = f.contentDocument;
  doc.body.firstChild;
  let split = t[1]
  let text1 = doc.body.childNodes[split[0]]
  let text2 = doc.body.childNodes[split[0]+1]
  if (text2.textContent.length==0) return;
  let c = doc.createTextNode(text2.textContent[0]);
  doc.body.insertBefore(c,text2);
  let r = doc.createRange();
  r.setStart(text2, 0);
  r.setEnd(text2, 1);
  r.deleteContents();
}
function test_split_merge(f,t) {
  runtest(f,t);
  f.contentDocument.body.normalize();
}
function test_merge(f,t) {
  runtest(f,t,true);
  f.contentDocument.body.normalize();
}

function repaint_selection(win) {
  let a = new Array;
  let sel = win.getSelection();
  for (let i = 0; i < sel.rangeCount; ++i) {
    a[i] = sel.getRangeAt(i);
  }
  sel.removeAllRanges();
  for (let i = 0; i < a.length; ++i) {
    sel.addRange(a[i]);
  }
}

function createFrame(run,t) {
  let f = document.createElement('iframe');
  f.setAttribute('height','22');
  f.setAttribute('frameborder','0');
  f.setAttribute('width','200');
  f.src = 'data:text/html,<body style="margin:0;padding:0">';
  f.onload = function () { try { run(f, t); repaint_selection(f.contentWindow);} finally { ++tests_done; } }
  return f;
}
