# @(#)conv.sed	8.6 (Sleepycat) 9/18/97
#
# Convert output into man page references.

# Discard empty lines.
/^$/d

# Discard START/STOP lines.
/^    !START/d
/^    !STOP/d

# Function names get a leading keyword.
/^ /!{
	s/^/FUNCTION /
	p
	d
}

# Functions that never return an error.
/^    !_exit$/d
/^    !abort$/d
/^    !ctime$/d
/^    !fprintf$/d
/^    !free$/d
/^    !getenv$/d
/^    !getpid$/d
/^    !getuid$/d
/^    !isalpha$/d
/^    !isdigit$/d
/^    !isprint$/d
/^    !printf$/d
/^    !snprintf$/d
/^    !sprintf$/d
/^    !vfprintf$/d
/^    !vsnprintf$/d

# Section 2 list from BSD/OS.
s/    !_exit$/_exit(2),/
s/    !accept$/accept(2),/
s/    !access$/access(2),/
s/    !acct$/acct(2),/
s/    !adjtime$/adjtime(2),/
s/    !bind$/bind(2),/
s/    !brk$/brk(2),/
s/    !chdir$/chdir(2),/
s/    !chflags$/chflags(2),/
s/    !chmod$/chmod(2),/
s/    !chown$/chown(2),/
s/    !chroot$/chroot(2),/
s/    !close$/close(2),/
s/    !connect$/connect(2),/
s/    !dup$/dup(2),/
s/    !dup2$/dup2(2),/
s/    !exec$/exec(2),/
s/    !execve$/execve(2),/
s/    !fchdir$/fchdir(2),/
s/    !fchflags$/fchflags(2),/
s/    !fchmod$/fchmod(2),/
s/    !fchown$/fchown(2),/
s/    !fcntl$/fcntl(2),/
s/    !fd_alloc$/fd_alloc(2),/
s/    !fd_copy$/fd_copy(2),/
s/    !fd_realloc$/fd_realloc(2),/
s/    !fd_zero$/fd_zero(2),/
s/    !flock$/flock(2),/
s/    !fork$/fork(2),/
s/    !fpathconf$/fpathconf(2),/
s/    !free$/free(2),/
s/    !fstat$/fstat(2),/
s/    !fstatfs$/fstatfs(2),/
s/    !fsync$/fsync(2),/
s/    !ftok$/ftok(2),/
s/    !ftruncate$/ftruncate(2),/
s/    !getdirentries$/getdirentries(2),/
s/    !getdtablesize$/getdtablesize(2),/
s/    !getegid$/getegid(2),/
s/    !geteuid$/geteuid(2),/
s/    !getfh$/getfh(2),/
s/    !getfsstat$/getfsstat(2),/
s/    !getgid$/getgid(2),/
s/    !getgroups$/getgroups(2),/
s/    !getitimer$/getitimer(2),/
s/    !getlogin$/getlogin(2),/
s/    !getpeername$/getpeername(2),/
s/    !getpgrp$/getpgrp(2),/
s/    !getpid$/getpid(2),/
s/    !getppid$/getppid(2),/
s/    !getpriority$/getpriority(2),/
s/    !getrlimit$/getrlimit(2),/
s/    !getrusage$/getrusage(2),/
s/    !getsockname$/getsockname(2),/
s/    !getsockopt$/getsockopt(2),/
s/    !gettimeofday$/gettimeofday(2),/
s/    !getuid$/getuid(2),/
s/    !ioctl$/ioctl(2),/
s/    !kill$/kill(2),/
s/    !ktrace$/ktrace(2),/
s/    !link$/link(2),/
s/    !listen$/listen(2),/
s/    !lseek$/lseek(2),/
s/    !lstat$/lstat(2),/
s/    !madvise$/madvise(2),/
s/    !mincore$/mincore(2),/
s/    !mkdir$/mkdir(2),/
s/    !mkfifo$/mkfifo(2),/
s/    !mknod$/mknod(2),/
s/    !mlock$/mlock(2),/
s/    !mmap$/mmap(2),/
s/    !mount$/mount(2),/
s/    !mprotect$/mprotect(2),/
s/    !msgctl$/msgctl(2),/
s/    !msgget$/msgget(2),/
s/    !msgrcv$/msgrcv(2),/
s/    !msgsnd$/msgsnd(2),/
s/    !msync$/msync(2),/
s/    !munlock$/munlock(2),/
s/    !munmap$/munmap(2),/
s/    !nfssvc$/nfssvc(2),/
s/    !open$/open(2),/
s/    !pathconf$/pathconf(2),/
s/    !pipe$/pipe(2),/
s/    !profil$/profil(2),/
s/    !profile$/profile(2),/
s/    !ptrace$/ptrace(2),/
s/    !quotactl$/quotactl(2),/
s/    !read$/read(2),/
s/    !readlink$/readlink(2),/
s/    !readv$/readv(2),/
s/    !reboot$/reboot(2),/
s/    !recv$/recv(2),/
s/    !recvfrom$/recvfrom(2),/
s/    !recvmsg$/recvmsg(2),/
s/    !rename$/rename(2),/
s/    !rmdir$/rmdir(2),/
s/    !sbrk$/sbrk(2),/
s/    !select$/select(2),/
s/    !semctl$/semctl(2),/
s/    !semget$/semget(2),/
s/    !semop$/semop(2),/
s/    !send$/send(2),/
s/    !sendmsg$/sendmsg(2),/
s/    !sendto$/sendto(2),/
s/    !setegid$/setegid(2),/
s/    !seteuid$/seteuid(2),/
s/    !setgid$/setgid(2),/
s/    !setgroups$/setgroups(2),/
s/    !setitimer$/setitimer(2),/
s/    !setlogin$/setlogin(2),/
s/    !setpgid$/setpgid(2),/
s/    !setpgrp$/setpgrp(2),/
s/    !setpriority$/setpriority(2),/
s/    !setrlimit$/setrlimit(2),/
s/    !setsockopt$/setsockopt(2),/
s/    !settimeofday$/settimeofday(2),/
s/    !setuid$/setuid(2),/
s/    !setuid.2:.pq$/setuid.2:.pq(2),/
s/    !shmat$/shmat(2),/
s/    !shmctl$/shmctl(2),/
s/    !shmdt$/shmdt(2),/
s/    !shmget$/shmget(2),/
s/    !shutdown$/shutdown(2),/
s/    !sigaction$/sigaction(2),/
s/    !sigaltstack$/sigaltstack(2),/
s/    !sigprocmask$/sigprocmask(2),/
s/    !sigreturn$/sigreturn(2),/
s/    !sigstack$/sigstack(2),/
s/    !sigsuspend$/sigsuspend(2),/
s/    !socket$/socket(2),/
s/    !socketpair$/socketpair(2),/
s/    !stat$/stat(2),/
s/    !statfs$/statfs(2),/
s/    !swapon$/swapon(2),/
s/    !sync$/sync(2),/
s/    !syscall$/syscall(2),/
s/    !tcgetpgrp$/tcgetpgrp(2),/
s/    !tcsetpgrp$/tcsetpgrp(2),/
s/    !truncate$/truncate(2),/
s/    !umask$/umask(2),/
s/    !undelete$/undelete(2),/
s/    !unlink$/unlink(2),/
s/    !unmount$/unmount(2),/
s/    !utimes$/utimes(2),/
s/    !vfork$/vfork(2),/
s/    !wait$/wait(2),/
s/    !wait3$/wait3(2),/
s/    !wait4$/wait4(2),/
s/    !waitpid$/waitpid(2),/
s/    !wcoredump$/wcoredump(2),/
s/    !wifexited$/wifexited(2),/
s/    !wifsignaled$/wifsignaled(2),/
s/    !wifstopped$/wifstopped(2),/
s/    !write$/write(2),/
s/    !writev$/writev(2),/

# Anything else is section 3.
/(2)/!{
	s/ *!\(.*\)/\1(3),/
}
