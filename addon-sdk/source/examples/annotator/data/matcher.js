/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Locate anchors for annotations and prepare to display the annotations.

For each annotation, if its URL matches this page,
- get the ancestor whose ID matches the ID in the anchor
- look for a <p> element whose content contains the anchor text

That's considered a match. Then we:
- highlight the anchor element
- add an 'annotated' class to tell the selector to skip this element
- embed the annottion text as a new attribute

For all annotated elements:
- bind 'mouseenter' and 'mouseleave' events to the element, to send 'show'
  and 'hide' messages back to the add-on.
*/

self.on('message', function onMessage(annotations) {
  annotations.forEach(
    function(annotation) {
      if(annotation.url == document.location.toString()) {
        createAnchor(annotation);
      }
  });

  $('.annotated').css('border', 'solid 3px yellow');

  $('.annotated').bind('mouseenter', function(event) {
    self.port.emit('show', $(this).attr('annotation'));
    event.stopPropagation();
    event.preventDefault();
  });

  $('.annotated').bind('mouseleave', function() {
    self.port.emit('hide');
  });
});


function createAnchor(annotation) {
  annotationAnchorAncestor = $('#' + annotation.ancestorId)[0] || document.body;
  annotationAnchor = $(annotationAnchorAncestor).parent().find(
                     ':contains("' + annotation.anchorText + '")').last();
  annotationAnchor.addClass('annotated');
  annotationAnchor.attr('annotation', annotation.annotationText);
}
