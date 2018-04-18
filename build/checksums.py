#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement

from optparse import OptionParser
import hashlib
import logging
import os

logger = logging.getLogger('checksums.py')

def digest_file(filename, digest, chunk_size=131072):
    '''Produce a checksum for the file specified by 'filename'.  'filename'
    is a string path to a file that is opened and read in this function.  The
    checksum algorithm is specified by 'digest' and is a valid OpenSSL
    algorithm.  If the digest used is not valid or Python's hashlib doesn't
    work, the None object will be returned instead.  The size of blocks
    that this function will read from the file object it opens based on
    'filename' can be specified by 'chunk_size', which defaults to 1K'''
    assert not os.path.isdir(filename), 'this function only works with files'

    logger.debug('Creating new %s object' % digest)
    h = hashlib.new(digest)
    with open(filename, 'rb') as f:
        while True:
            data = f.read(chunk_size)
            if not data:
                logger.debug('Finished reading in file')
                break
            h.update(data)
    hash = h.hexdigest()
    logger.debug('Hash for %s is %s' % (filename, hash))
    return hash


def process_files(files, output_filename, digests, strip):
    '''This function takes a list of file names, 'files'.  It will then
    compute the checksum for each of the files by opening the files.
    Once each file is read and its checksum is computed, this function
    will write the information to the file specified by 'output_filename'.
    The path written in the output file will have anything specified by 'strip'
    removed from the path.  The output file is closed before returning nothing
    The algorithm to compute checksums with can be specified by 'digests'
    and needs to be a list of valid OpenSSL algorithms.

    The output file is written in the format:
        <hash> <algorithm> <filesize> <filepath>
    Example:
        d1fa09a<snip>e4220 sha1 14250744 firefox-4.0b6pre.en-US.mac64.dmg
    '''

    if os.path.exists(output_filename):
        logger.debug('Overwriting existing checksums file "%s"' %
                     output_filename)
    else:
        logger.debug('Creating a new checksums file "%s"' % output_filename)
    with open(output_filename, 'w+') as output:
        for file in files:
            for digest in digests:
                hash = digest_file(file, digest)

                if file.startswith(strip):
                    short_file = file[len(strip):]
                    short_file = short_file.lstrip('/')
                else:
                    short_file = file
                print >>output, '%s %s %s %s' % (hash, digest,
                                                 os.path.getsize(file),
                                                 short_file)


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
                      action='append', dest='digests')
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

    # Figure out which logging level to use
    if options.verbose:
        loglevel = logging.DEBUG
    elif options.quiet:
        loglevel = logging.ERROR
    else:
        loglevel = logging.INFO

    # Set up logging
    setup_logging(loglevel)

    # Validate the digest type to use
    if not options.digests:
        options.digests = ['sha1']

    # Validate the files to checksum
    files = []
    for i in args:
        if os.path.isdir(i):
            logger.warn('%s is a directory; ignoring' % i)
        elif os.path.exists(i):
            files.append(i)
        else:
            logger.info('File "%s" was not found on the filesystem' % i)
    process_files(files, options.outfile, options.digests, options.strip)


if __name__ == '__main__':
    main()
