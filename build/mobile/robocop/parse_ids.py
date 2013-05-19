# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import os
import sys
import optparse

def getFile(filename):
  fHandle = open(filename, 'r')
  data = fHandle.read()
  fHandle.close()
  return data
  
def findIDs(data):
  start_function = False
  reID = re.compile('.*public static final class id {.*')
  reEnd = re.compile('.*}.*')
  idlist = []
  
  for line in data.split('\n'):
    if reEnd.match(line):
      start_function = False
      
    if start_function:
      id_value = line.split(' ')[-1]
      idlist.append(id_value.split(';')[0].split('='))
      
    if reID.match(line):
      start_function = True
      
  return idlist
  
  
def printIDs(outputFile, idlist):
  fOutput = open(outputFile, 'w')
  for item in idlist:
    fOutput.write("%s=%s\n" % (item[0], item[1]))
  fOutput.close()

def main(args=sys.argv[1:]):
  parser = optparse.OptionParser()
  parser.add_option('-o', '--output', dest='outputFile', default='',
                    help="output file with the id=value pairs")
  parser.add_option('-i', '--input', dest='inputFile', default='',
                    help="filename of the input R.java file")
  options, args = parser.parse_args(args)
  
  if options.inputFile == '':
    print "Error: please provide input file: -i <filename>"
    sys.exit(1)

  if options.outputFile == '':
    print "Error: please provide output file: -o <filename>"
    sys.exit(1)

  data = getFile(os.path.abspath(options.inputFile));
  idlist = findIDs(data)
  printIDs(os.path.abspath(options.outputFile), idlist)

if __name__ == "__main__":
    main()

