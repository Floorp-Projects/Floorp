function buildTable()
{
    var seed = 0;

    var neutrals = ['"', ")", "("];
    var strongRTLs = ['א', 'ב', 'ג', 'ד', 'ה', 'ו', 'ז'];
    var strongLTRs = ['a', 'b', 'c', 'd', 'e', 'f', 'g'];
    var neutral = function() { return neutrals[seed++ % neutrals.length]; }
    var strongRTL = function() { return strongRTLs[seed++ % strongRTLs.length]; }
    var strongLTR = function() { return strongLTRs[seed++ % strongLTRs.length]; }
    var charClassExamples = [neutral, strongRTL, strongLTR];
    var possibleDirs = ['ltr', 'rtl'];

    var elem=document.getElementById("elem");
    for (outerDirIndex in possibleDirs) {
	var outerDir = possibleDirs[outerDirIndex];
	for (beforeSpanIndex in charClassExamples) {
            var beforeSpan = charClassExamples[beforeSpanIndex];
            for (spanDirIndex in possibleDirs) {
		var spanDir = possibleDirs[spanDirIndex];
		for (inSpanIndex in charClassExamples) {
                    var inSpan = charClassExamples[inSpanIndex];
                    for (afterSpanIndex in charClassExamples) {
			var afterSpan = charClassExamples[afterSpanIndex];
			function caseWithStyle() {
                            seed = 0;
                            var outerDiv = document.createElement("div");
                            outerDiv.dir = outerDir;
                            outerDiv.appendChild(document.createTextNode(beforeSpan()));
                            var span = document.createElement("span");
                            span.dir = spanDir;
                            span.setAttribute("class", "enclosed")
                            span.appendChild(document.createTextNode(inSpan()));
                            outerDiv.appendChild(span);
                            outerDiv.appendChild(document.createTextNode(afterSpan()));
                            return outerDiv;
			}
			elem.appendChild(caseWithStyle());
                    }
		}
            }
	}
    }
}
