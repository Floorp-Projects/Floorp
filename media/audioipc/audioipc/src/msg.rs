use libc;
use std::io;
use std::mem;
use std::os::unix::io::RawFd;
use std::ptr;

// Note: The following fields must be laid out together, the OS expects them
// to be part of a single allocation.
#[repr(C)]
struct CmsgSpace {
    cmsghdr: libc::cmsghdr,
    #[cfg(not(target_os = "macos"))]
    __padding: [usize; 0],
    data: libc::c_int,
}

#[cfg(not(target_os = "macos"))]
fn cmsg_align(len: usize) -> usize {
    let align_bytes = mem::size_of::<usize>() - 1;
    (len + align_bytes) & !align_bytes
}

#[cfg(target_os = "macos")]
fn cmsg_align(len: usize) -> usize {
    len
}

fn cmsg_space() -> usize {
    mem::size_of::<CmsgSpace>()
}

fn cmsg_len() -> usize {
    cmsg_align(mem::size_of::<libc::cmsghdr>()) + mem::size_of::<libc::c_int>()
}

pub fn sendmsg(fd: RawFd, to_send: &[u8], fd_to_send: Option<RawFd>) -> io::Result<usize> {
    let mut msghdr: libc::msghdr = unsafe { mem::zeroed() };
    let mut iovec: libc::iovec = unsafe { mem::zeroed() };
    let mut cmsg: CmsgSpace = unsafe { mem::zeroed() };

    msghdr.msg_iov = &mut iovec as *mut _;
    msghdr.msg_iovlen = 1;
    if fd_to_send.is_some() {
        msghdr.msg_control = &mut cmsg.cmsghdr as *mut _ as *mut _;
        msghdr.msg_controllen = cmsg_space() as _;
    }

    iovec.iov_base = if to_send.is_empty() {
        // Empty Vecs have a non-null pointer.
        ptr::null_mut()
    } else {
        to_send.as_ptr() as *const _ as *mut _
    };
    iovec.iov_len = to_send.len();

    cmsg.cmsghdr.cmsg_len = cmsg_len() as _;
    cmsg.cmsghdr.cmsg_level = libc::SOL_SOCKET;
    cmsg.cmsghdr.cmsg_type = libc::SCM_RIGHTS;

    cmsg.data = fd_to_send.unwrap_or(-1);

    let result = unsafe { libc::sendmsg(fd, &msghdr, 0) };
    if result >= 0 {
        Ok(result as usize)
    } else {
        Err(io::Error::last_os_error())
    }
}

pub fn recvmsg(fd: RawFd, to_recv: &mut [u8]) -> io::Result<(usize, Option<RawFd>)> {
    let mut msghdr: libc::msghdr = unsafe { mem::zeroed() };
    let mut iovec: libc::iovec = unsafe { mem::zeroed() };
    let mut cmsg: CmsgSpace = unsafe { mem::zeroed() };

    msghdr.msg_iov = &mut iovec as *mut _;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = &mut cmsg.cmsghdr as *mut _ as *mut _;
    msghdr.msg_controllen = cmsg_space() as _;

    iovec.iov_base = if to_recv.is_empty() {
        // Empty Vecs have a non-null pointer.
        ptr::null_mut()
    } else {
        to_recv.as_ptr() as *const _ as *mut _
    };
    iovec.iov_len = to_recv.len();

    let result = unsafe { libc::recvmsg(fd, &mut msghdr, 0) };
    if result >= 0 {
        let fd = if msghdr.msg_controllen == cmsg_space() as _ &&
            cmsg.cmsghdr.cmsg_len == cmsg_len() as _ &&
            cmsg.cmsghdr.cmsg_level == libc::SOL_SOCKET &&
            cmsg.cmsghdr.cmsg_type == libc::SCM_RIGHTS {
                Some(cmsg.data)
            } else {
                None
            };

        Ok((result as usize, fd))
    } else {
        Err(io::Error::last_os_error())
    }
}

#[cfg(test)]
mod tests {
    use libc;
    use std::mem;
    use std::os::unix::net::UnixStream;
    use std::os::unix::io::{AsRawFd, FromRawFd, IntoRawFd};
    use std::io::{Read, Write};
    use super::{cmsg_len, cmsg_space, sendmsg, recvmsg};

    #[test]
    fn portable_sizes() {
        if cfg!(all(target_os = "linux", target_pointer_width = "64")) {
            assert_eq!(mem::size_of::<libc::cmsghdr>(), 16);
            assert_eq!(cmsg_len(), 20);
            assert_eq!(cmsg_space(), 24);
        } else if cfg!(all(target_os = "linux", target_pointer_width = "32")) {
            assert_eq!(mem::size_of::<libc::cmsghdr>(), 12);
            assert_eq!(cmsg_len(), 16);
            assert_eq!(cmsg_space(), 16);
        } else if cfg!(target_os = "macos") {
            assert_eq!(mem::size_of::<libc::cmsghdr>(), 12);
            assert_eq!(cmsg_len(), 16);
            assert_eq!(cmsg_space(), 16);
        } else if cfg!(target_pointer_width = "64") {
            assert_eq!(mem::size_of::<libc::cmsghdr>(), 12);
            assert_eq!(cmsg_len(), 20);
            assert_eq!(cmsg_space(), 24);
        } else {
            assert_eq!(mem::size_of::<libc::cmsghdr>(), 12);
            assert_eq!(cmsg_len(), 16);
            assert_eq!(cmsg_space(), 16);
        }
    }

    #[test]
    fn fd_passing() {
        let (tx, rx) = UnixStream::pair().unwrap();

        let (send_tx, mut send_rx) = UnixStream::pair().unwrap();

        let fd = send_tx.into_raw_fd();
        assert_eq!(sendmsg(tx.as_raw_fd(), b"a", Some(fd)).unwrap(), 1);
        unsafe { libc::close(fd) };

        let mut buf = [0u8];
        let (got, fd) = recvmsg(rx.as_raw_fd(), &mut buf).unwrap();
        assert_eq!(got, 1);
        assert_eq!(&buf, b"a");

        let mut send_tx = unsafe { UnixStream::from_raw_fd(fd.unwrap()) };
        assert_eq!(send_tx.write(b"b").unwrap(), 1);

        let mut buf = [0u8];
        assert_eq!(send_rx.read(&mut buf).unwrap(), 1);
        assert_eq!(&buf, b"b");
    }
}
