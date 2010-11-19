#!/usr/bin/python
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is a checksum generation utility.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#    John Ford <jhford@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

from optparse import OptionParser
import logging
import os
try:
    import hashlib
except:
    hashlib = None

def digest_file(filename, digest, chunk_size=1024):
    '''Produce a checksum for the file specified by 'filename'.  'filename'
    is a string path to a file that is opened and read in this function.  The
    checksum algorithm is specified by 'digest' and is a valid OpenSSL
    algorithm.  If the digest used is not valid or Python's hashlib doesn't
    work, the None object will be returned instead.  The size of blocks
    that this function will read from the file object it opens based on
    'filename' can be specified by 'chunk_size', which defaults to 1K'''
    assert not os.path.isdir(filename), 'this function only works with files'
    logger = logging.getLogger('checksums.py')
    if hashlib is not None:
        logger.debug('Creating new %s object' % digest)
        h = hashlib.new(digest)
        f = open(filename)
        while True:
            data = f.read(chunk_size)
            if not data:
                logger.debug('Finished reading in file')
                break
            h.update(data)
        f.close()
        hash = h.hexdigest()
        logger.debug('Hash for %s is %s' % (filename, hash))
        return hash
    else:
        # In this case we could subprocess.Popen and .communicate with
        # sha1sum or md5sum
        logger.warn('The python module for hashlib is missing!')
        return None


def process_files(files, output_filename, digest, strip):
    '''This function takes a list of file names, 'files'.  It will then
    compute the checksum for each of the files by opening the files.
    Once each file is read and its checksum is computed, this function
    will write the information to the file specified by 'output_filename'.
    The path written in the output file will have anything specified by 'strip'
    removed from the path.  The output file is closed before returning nothing
    The algorithm to compute checksums with can be specified by 'digest' 
    and needs to be a valid OpenSSL algorithm.

    The output file is written in the format:
        <hash> <algorithm> <filesize> <filepath>
    Example:
        d1fa09a<snip>e4220 sha1 14250744 firefox-4.0b6pre.en-US.mac64.dmg
    '''

    logger = logging.getLogger('checksums.py')
    if os.path.exists(output_filename):
        logger.debug('Overwriting existing checksums file "%s"' %
                     output_filename)
    else:
        logger.debug('Creating a new checksums file "%s"' % output_filename)
    output = open(output_filename, 'w+')
    for file in files:
        if os.path.isdir(file):
            logger.warn('%s is a directory, skipping' % file)
        else:
            hash = digest_file(file, digest)
            if hash is None:
                logger.warn('Unable to generate a hash for %s. ' +
                            'Using NOHASH as fallback' % file)
                hash = 'NOHASH'
            if file.startswith(strip):
                short_file = file[len(strip):]
                short_file = short_file.lstrip('/')
            else:
                short_file = file
            print >>output, '%s %s %s %s' % (hash, digest,
                                             os.path.getsize(file),
                                             short_file)
    output.close()

def setup_logging(level=logging.DEBUG):
    '''This function sets up the logging module using a speficiable logging
    module logging level.  The default log level is DEBUG.

    The output is in the format:
        <level> - <message>
    Example:
        DEBUG - Finished reading in file
'''

    logger = logging.getLogger('checksums.py')
    logger.setLevel(logging.DEBUG)
    handler = logging.StreamHandler()
    handler.setLevel(level)
    formatter = logging.Formatter("%(levelname)s - %(message)s")
    handler.setFormatter(formatter)
    logger.addHandler(handler)

def main():
    '''This is a main function that parses arguments, sets up logging
    and generates a checksum file'''
    # Parse command line arguments
    parser = OptionParser()
    parser.add_option('-d', '--digest', help='checksum algorithm to use',
                      action='store', dest='digest', default='sha1')
    parser.add_option('-o', '--output', help='output file to use',
                      action='store', dest='outfile', default='checksums')
    parser.add_option('-v', '--verbose',
                      help='Be noisy (takes precedence over quiet)',
                      action='store_true', dest='verbose', default=False)
    parser.add_option('-q', '--quiet', help='Be quiet', action='store_true',
                      dest='quiet', default=False)
    parser.add_option('-s', '--strip',
                      help='strip this path from the filenames',
                      dest='strip', default=os.getcwd())
    options, args = parser.parse_args()

    #Figure out which logging level to use
    if options.verbose:
        loglevel = logging.DEBUG
    elif options.quiet:
        loglevel = logging.ERROR
    else:
        loglevel = logging.INFO

    #Set up logging
    setup_logging(loglevel)
    logger = logging.getLogger('checksums.py')

    # Validate the digest type to use
    try:
        hashlib.new(options.digest)
    except ValueError, ve:
        logger.error('Could not create a "%s" hash object (%s)' %
                     (options.digest, ve.args[0]))
        exit(1)

    # Validate the files to checksum
    files = []
    for i in args:
        if os.path.exists(i):
            files.append(i)
        else:
            logger.info('File "%s" was not found on the filesystem' % i)
    process_files(files, options.outfile, options.digest, options.strip)

if __name__ == '__main__':
    main()
