const DEBUG_all_valid = false;
const DEBUG_all_stub = false;

function handleRequest(request, response)
{
    // Decode the query string to know what test we're doing.

    // character 1: 'I' = text/css response, 'J' = text/html response
    let responseCSS = (request.queryString[0] == 'I');

    // character 2: redirection type - we only care about whether we're
    // ultimately same-origin with the requesting document ('A', 'D') or
    // not ('B', 'C').
    let sameOrigin = (request.queryString[1] == 'A' ||
		      request.queryString[1] == 'D');

    // character 3: '1' = syntactically valid, '2' = invalid, '3' = http error
    let malformed = (request.queryString[2] == '2');
    let httpError = (request.queryString[2] == '3');

    // character 4: loaded with <link> or @import (no action required)

    // character 5: loading document mode: 'q' = quirks, 's' = standards
    let quirksMode = (request.queryString[4] == 'q');

    // Our response contains a CSS rule that selects an element whose
    // ID is the first four characters of the query string.
    let selector = '#' + request.queryString.substring(0,4);

    // "Malformed" responses wrap the CSS rule in the construct
    //     <html>{} ... </html>
    // This mimics what the CSS parser might see if an actual HTML
    // document were fed to it.  Because CSS parsers recover from
    // errors by skipping tokens until they find something
    // recognizable, a style rule appearing where I wrote '...' above
    // will be honored!
    let leader = (malformed ? '<html>{}' : '');
    let trailer = (malformed ? '</html>' : '');

    // Standards mode documents will ignore the style sheet if it is being
    // served as text/html (regardless of its contents).  Quirks mode
    // documents will ignore the style sheet if it is being served as
    // text/html _and_ it is not same-origin.  Regardless, style sheets
    // are ignored if they come as the body of an HTTP error response.
    //
    // Style sheets that should be ignored paint the element red; those
    // that should be honored paint it lime.
    let color = ((responseCSS || (quirksMode && sameOrigin)) && !httpError
		 ? 'lime' : 'red');

    // For debugging the test itself, we have the capacity to make every style
    // sheet well-formed, or every style sheet do nothing.
    if (DEBUG_all_valid) {
	// In this mode, every test chip should turn blue.
	response.setHeader('Content-Type', 'text/css');
	response.write(selector + '{background-color:blue}\n');
    } else if (DEBUG_all_stub) {
	// In this mode, every test chip for a case where the true test
	// sheet would be honored, should turn red.
	response.setHeader('Content-Type', 'text/css');
	response.write(selector + '{}\n');
    } else {
	// Normal operation.
	if (httpError)
	    response.setStatusLine(request.httpVersion, 500,
				   "Internal Server Error");
	response.setHeader('Content-Type',
			   responseCSS ? 'text/css' : 'text/html');
	response.write(leader + selector +
		       '{background-color:' + color + '}' +
		       trailer + '\n');
    }
}
