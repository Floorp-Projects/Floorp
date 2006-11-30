import os

def run_for_output(cmd):
    if type(cmd) == list:
	# XXX need to escape arguments better... am I in a shell?
	cmd = " ".join(cmd)

    pipe = os.popen(cmd, "r")
    output = pipe.read()
    exitval = pipe.close()
    if (exitval):
	raise Exception("Command failed: '" + cmd + "'")

    return output
