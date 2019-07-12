pub trait AsEndMutPtr<T> {
    fn as_end_mut_ptr(self) -> *mut T;
}

impl<'a> AsEndMutPtr<u8> for &'a mut [u8] {
    fn as_end_mut_ptr(self) -> *mut u8 {
        unsafe { self.as_mut_ptr().add(self.len()) }
    }
}
