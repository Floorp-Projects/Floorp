/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Construct the HTML for the annotation list.

Bind a function to click events on the link that send a message back to
the add-on code, so it can open the link in the main browser.
*/

self.on("message", function onMessage(storedAnnotations) {
  var annotationList = $('#annotation-list');
  annotationList.empty();
  storedAnnotations.forEach(
    function(storedAnnotation) {
      var annotationHtml = $('#template .annotation-details').clone();
      annotationHtml.find('.url').text(storedAnnotation.url)
                                 .attr('href', storedAnnotation.url);
      annotationHtml.find('.url').bind('click', function(event) {
        event.stopPropagation();
        event.preventDefault();
        self.postMessage(storedAnnotation.url);
      });
      annotationHtml.find('.selection-text')
                    .text(storedAnnotation.anchorText);
      annotationHtml.find('.annotation-text')
                    .text(storedAnnotation.annotationText);
      annotationList.append(annotationHtml);
    });
});
