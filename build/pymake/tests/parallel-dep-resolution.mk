#T commandline: ['-j3']
#T returncode: 2

all: t1 t2

t1:
	sleep 1
	touch t1 t2
