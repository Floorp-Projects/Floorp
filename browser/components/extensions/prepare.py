#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import json
import uuid
import sys
import os.path

parser = argparse.ArgumentParser(description='Create install.rdf from manifest.json')
parser.add_argument('--locale')
parser.add_argument('--profile')
parser.add_argument('--uuid')
parser.add_argument('dir')
args = parser.parse_args()

manifestFile = os.path.join(args.dir, 'manifest.json')
manifest = json.load(open(manifestFile))

locale = args.locale
if not locale:
    locale = manifest.get('default_locale', 'en-US')

def process_locale(s):
    if s.startswith('__MSG_') and s.endswith('__'):
        tag = s[6:-2]
        path = os.path.join(args.dir, '_locales', locale, 'messages.json')
        data = json.load(open(path))
        return data[tag]['message']
    else:
        return s

id = args.uuid
if not id:
    id = '{' + str(uuid.uuid4()) + '}'

name = process_locale(manifest['name'])
desc = process_locale(manifest['description'])
version = manifest['version']

installFile = open(os.path.join(args.dir, 'install.rdf'), 'w')
print >>installFile, '<?xml version="1.0"?>'
print >>installFile, '<RDF xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#"'
print >>installFile, '     xmlns:em="http://www.mozilla.org/2004/em-rdf#">'
print >>installFile
print >>installFile, '  <Description about="urn:mozilla:install-manifest">'
print >>installFile, '    <em:id>{}</em:id>'.format(id)
print >>installFile, '    <em:type>2</em:type>'
print >>installFile, '    <em:name>{}</em:name>'.format(name)
print >>installFile, '    <em:description>{}</em:description>'.format(desc)
print >>installFile, '    <em:version>{}</em:version>'.format(version)
print >>installFile, '    <em:bootstrap>true</em:bootstrap>'

print >>installFile, '    <em:targetApplication>'
print >>installFile, '      <Description>'
print >>installFile, '        <em:id>{ec8030f7-c20a-464f-9b0e-13a3a9e97384}</em:id>'
print >>installFile, '        <em:minVersion>4.0</em:minVersion>'
print >>installFile, '        <em:maxVersion>50.0</em:maxVersion>'
print >>installFile, '      </Description>'
print >>installFile, '    </em:targetApplication>'

print >>installFile, '  </Description>'
print >>installFile, '</RDF>'
installFile.close()

bootstrapPath = os.path.join(os.path.dirname(sys.argv[0]), 'bootstrap.js')
data = open(bootstrapPath).read()
boot = open(os.path.join(args.dir, 'bootstrap.js'), 'w')
boot.write(data)
boot.close()

if args.profile:
    os.system('mkdir -p {}/extensions'.format(args.profile))
    output = open(args.profile + '/extensions/' + id, 'w')
    print >>output, os.path.realpath(args.dir)
    output.close()
else:
    dir = os.path.realpath(args.dir)
    if dir[-1] == os.sep:
        dir = dir[:-1]
    os.system('cd "{}"; zip ../"{}".xpi -r *'.format(args.dir, os.path.basename(dir)))
