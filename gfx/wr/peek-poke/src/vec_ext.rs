use std::vec::Vec;

pub trait VecExt  {
    type Item;
    unsafe fn set_end_ptr(&mut self, end: *const Self::Item);
}

impl<T> VecExt for Vec<T> {
    type Item = T;
    unsafe fn set_end_ptr(&mut self, end: *const T) {
        assert!(end as usize >= self.as_ptr() as usize);
        let new_len = end as usize - self.as_ptr() as usize;
        assert!(new_len <= self.capacity());
        self.set_len(new_len);
    }
}
