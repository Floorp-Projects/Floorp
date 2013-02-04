/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
The selector locates elements that are suitable for annotation and enables
the user to select them.

On 'mouseenter' events associated with <p> elements:
- if the selector is active and the element is not already annotated
- find the nearest ancestor which has an id attribute: this is supposed to
make identification of this element more accurate
- highlight the element
- bind 'click' for the element to send a message back to the add-on, including
all the information associated with the anchor.
*/

var matchedElement = null;
var originalBgColor = null;
var active = false;

function resetMatchedElement() {
  if (matchedElement) {
    matchedElement.css('background-color', originalBgColor);
    matchedElement.unbind('click.annotator');
  }
}

self.on('message', function onMessage(activation) {
  active = activation;
  if (!active) {
    resetMatchedElement();
  }
});

function getInnerText(element) {
  // jQuery.text() returns content of <script> tags, we need to ignore those
  var list = [];
  element.find("*").andSelf().contents()
    .filter(function () {
      return this.nodeType == 3 && this.parentNode.tagName != "SCRIPT";
    })
    .each(function () {
      list.push(this.nodeValue);
    });
  return list.join("");
}

$('*').mouseenter(function() {
  if (!active || $(this).hasClass('annotated')) {
    return;
  }
  resetMatchedElement();
  ancestor = $(this).closest("[id]");
  matchedElement = $(this).first();
  originalBgColor = matchedElement.css('background-color');
  matchedElement.css('background-color', 'yellow');
  matchedElement.bind('click.annotator', function(event) {
    event.stopPropagation();
    event.preventDefault();
    self.port.emit('show',
      [
        document.location.toString(),
        ancestor.attr("id"),
        getInnerText(matchedElement)
      ]
   );
  });
});

$('*').mouseout(function() {
  resetMatchedElement();
});
