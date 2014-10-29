import sys
import os.path
import re

assert len(sys.argv) == 2
mochiPath = sys.argv[1]

extDotPos = mochiPath.find('.html')
assert extDotPos != -1, 'mochitest target must be an html doc.'

testPath = mochiPath[:extDotPos] + '.solo.html'

def ReadLocalFile(include):
    incPath = os.path.dirname(mochiPath)
    filePath = os.path.join(incPath, include)

    data = None
    try:
        f = open(filePath, 'r')
        data = f.read()
    except:
        pass

    try:
        f.close()
    except:
        pass

    return data

kSimpleTestReplacement = '''\n
<script>
// SimpleTest.js replacement

function debug(text) {
  var elem = document.getElementById('mochi-to-testcase-output');
  elem.innerHTML += '\\n<br/>\\n' + text;
}

function ok(val, text) {
  var status = val ? 'Test <font color=\\'green\\'>passed</font>: '
                   : 'Test <font color=\\'red\\'  >FAILED</font>: ';
  debug(status + text);
}

function todo(val, text) {
  var status = val ? 'Test <font color=\\'orange\\'>UNEXPECTED PASS</font>: '
                   : 'Test <font color=\\'blue\\'  >todo</font>: ';
  debug(status + text);
}
</script>
<div id='mochi-to-testcase-output'></div>
\n'''

fin = open(mochiPath, 'rb')
fout = open(testPath, 'wb')
includePattern = re.compile('<script\\s*src=[\'"](.*)\\.js[\'"]>\\s*</script>')
cssPattern = re.compile('<link\\s*rel=[\'"]stylesheet[\'"]\\s*href=[\'"]([^=>]*)[\'"]>')
for line in fin:
    skipLine = False
    for css in cssPattern.findall(line):
        skipLine = True
        print('Ignoring stylesheet: ' + css)

    for inc in includePattern.findall(line):
        skipLine = True
        if inc == '/MochiKit/MochiKit':
            continue

        if inc == '/tests/SimpleTest/SimpleTest':
            print('Injecting SimpleTest replacement')
            fout.write(kSimpleTestReplacement);
            continue

        incData = ReadLocalFile(inc + '.js')
        if not incData:
            print('Warning: Unknown JS file ignored: ' + inc + '.js')
            continue

        print('Injecting include: ' + inc + '.js')
        fout.write('\n<script>\n// Imported from: ' + inc + '.js\n');
        fout.write(incData);
        fout.write('\n</script>\n');
        continue

    if skipLine:
        continue

    fout.write(line)
    continue

fin.close()
fout.close()
