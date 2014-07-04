import subprocess as sp
import sys

def grab_output(*command):
    return sp.Popen(command, stdout=sp.PIPE).communicate()[0].strip()

commit_id_size = 12

try:
    commit_id = grab_output('git', 'rev-parse', '--short=%d' % commit_id_size, 'HEAD')
    commit_date = grab_output('git', 'show', '-s', '--format=%ci', 'HEAD')
except:
    commit_id = 'invalid-hash'
    commit_date = 'invalid-date'

hfile = open(sys.argv[1], 'w')

hfile.write('#define ANGLE_COMMIT_HASH "%s"\n'    % commit_id)
hfile.write('#define ANGLE_COMMIT_HASH_SIZE %d\n' % commit_id_size)
hfile.write('#define ANGLE_COMMIT_DATE "%s"\n'    % commit_date)

hfile.close()
