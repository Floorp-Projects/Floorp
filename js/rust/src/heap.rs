use glue;
use jsapi::root::*;
use rust::GCMethods;
use std::cell::UnsafeCell;
use std::ptr;

/// Types that can be traced.
///
/// This trait is unsafe; if it is implemented incorrectly, the GC may end up
/// collecting objects that are still reachable.
pub unsafe trait Trace {
    unsafe fn trace(&self, trc: *mut JSTracer);
}

/**
 * The Heap<T> class is a heap-stored reference to a JS GC thing. All members of
 * heap classes that refer to GC things should use Heap<T> (or possibly
 * TenuredHeap<T>, described below).
 *
 * Heap<T> is an abstraction that hides some of the complexity required to
 * maintain GC invariants for the contained reference. It uses operator
 * overloading to provide a normal pointer interface, but notifies the GC every
 * time the value it contains is updated. This is necessary for generational GC,
 * which keeps track of all pointers into the nursery.
 *
 * Heap<T> instances must be traced when their containing object is traced to
 * keep the pointed-to GC thing alive.
 *
 * Heap<T> objects should only be used on the heap. GC references stored on the
 * C/C++ stack must use Rooted/Handle/MutableHandle instead.
 *
 * Type T must be a public GC pointer type.
 */
#[repr(C)]
#[derive(Debug)]
pub struct Heap<T: GCMethods + Copy> {
    ptr: UnsafeCell<T>,
}

impl<T: GCMethods + Copy> Heap<T> {
    pub fn new(v: T) -> Heap<T>
        where Heap<T>: Default
    {
        let ptr = Heap::default();
        ptr.set(v);
        ptr
    }

    pub fn set(&self, v: T) {
        unsafe {
            let ptr = self.ptr.get();
            let prev = *ptr;
            *ptr = v;
            T::post_barrier(ptr, prev, v);
        }
    }

    pub fn get(&self) -> T {
        unsafe {
            *self.ptr.get()
        }
    }

    pub unsafe fn get_unsafe(&self) -> *mut T {
        self.ptr.get()
    }

    pub fn handle(&self) -> JS::Handle<T> {
        unsafe {
            JS::Handle::from_marked_location(self.ptr.get() as *const _)
        }
    }

    pub fn handle_mut(&self) -> JS::MutableHandle<T> {
        unsafe {
            JS::MutableHandle::from_marked_location(self.ptr.get())
        }
    }
}

impl<T: GCMethods + Copy> Clone for Heap<T>
    where Heap<T>: Default
{
    fn clone(&self) -> Self {
        Heap::new(self.get())
    }
}

impl<T: GCMethods + Copy + PartialEq> PartialEq for Heap<T> {
    fn eq(&self, other: &Self) -> bool {
        self.get() == other.get()
    }
}

impl<T> Default for Heap<*mut T>
    where *mut T: GCMethods + Copy
{
    fn default() -> Heap<*mut T> {
        Heap {
            ptr: UnsafeCell::new(ptr::null_mut())
        }
    }
}

impl Default for Heap<JS::Value> {
    fn default() -> Heap<JS::Value> {
        Heap {
            ptr: UnsafeCell::new(JS::Value::default())
        }
    }
}

impl<T: GCMethods + Copy> Drop for Heap<T> {
    fn drop(&mut self) {
        unsafe {
            let prev = self.ptr.get();
            T::post_barrier(prev, *prev, T::initial());
        }
    }
}

// Creates a C string literal `$str`.
macro_rules! c_str {
    ($str:expr) => {
        concat!($str, "\0").as_ptr() as *const ::std::os::raw::c_char
    }
}

unsafe impl Trace for Heap<*mut JSFunction> {
    unsafe fn trace(&self, trc: *mut JSTracer) {
        glue::CallFunctionTracer(trc, self as *const _ as *mut Self, c_str!("function"));
    }
}

unsafe impl Trace for Heap<*mut JSObject> {
    unsafe fn trace(&self, trc: *mut JSTracer) {
        glue::CallObjectTracer(trc, self as *const _ as *mut Self, c_str!("object"));
    }
}

unsafe impl Trace for Heap<*mut JSScript> {
    unsafe fn trace(&self, trc: *mut JSTracer) {
        glue::CallScriptTracer(trc, self as *const _ as *mut Self, c_str!("script"));
    }
}

unsafe impl Trace for Heap<*mut JSString> {
    unsafe fn trace(&self, trc: *mut JSTracer) {
        glue::CallStringTracer(trc, self as *const _ as *mut Self, c_str!("string"));
    }
}

unsafe impl Trace for Heap<JS::Value> {
    unsafe fn trace(&self, trc: *mut JSTracer) {
        glue::CallValueTracer(trc, self as *const _ as *mut Self, c_str!("value"));
    }
}

unsafe impl Trace for Heap<jsid> {
    unsafe fn trace(&self, trc: *mut JSTracer) {
        glue::CallIdTracer(trc, self as *const _ as *mut Self, c_str!("id"));
    }
}
