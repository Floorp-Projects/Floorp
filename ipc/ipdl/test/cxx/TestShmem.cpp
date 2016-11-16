#include "TestShmem.h"

#include "IPDLUnitTests.h"      // fail etc.


namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// Parent

void
TestShmemParent::Main()
{
    Shmem mem;
    Shmem unsafe;

    size_t size = 12345;
    if (!AllocShmem(size, SharedMemory::TYPE_BASIC, &mem))
        fail("can't alloc shmem");
    if (!AllocUnsafeShmem(size, SharedMemory::TYPE_BASIC, &unsafe))
        fail("can't alloc shmem");

    if (mem.Size<char>() != size)
        fail("shmem is wrong size: expected %lu, got %lu",
             size, mem.Size<char>());
    if (unsafe.Size<char>() != size)
        fail("shmem is wrong size: expected %lu, got %lu",
             size, unsafe.Size<char>());

    char* ptr = mem.get<char>();
    memcpy(ptr, "Hello!", sizeof("Hello!"));

    char* unsafeptr = unsafe.get<char>();
    memcpy(unsafeptr, "Hello!", sizeof("Hello!"));

    Shmem unsafecopy = unsafe;
    if (!SendGive(mem, unsafe, size))
        fail("can't send Give()");

    // uncomment the following line for a (nondeterministic) surprise!
    //char c1 = *ptr;  (void)c1;

    // uncomment the following line for a deterministic surprise!
    //char c2 = *mem.get<char>(); (void)c2;

    // unsafe shmem gets rid of those checks
    char uc1 = *unsafeptr;  (void)uc1;
    char uc2 = *unsafecopy.get<char>(); (void)uc2;
}


mozilla::ipc::IPCResult
TestShmemParent::RecvTake(Shmem&& mem, Shmem&& unsafe,
                          const size_t& expectedSize)
{
    if (mem.Size<char>() != expectedSize)
        fail("expected shmem size %lu, but it has size %lu",
             expectedSize, mem.Size<char>());
    if (unsafe.Size<char>() != expectedSize)
        fail("expected shmem size %lu, but it has size %lu",
             expectedSize, unsafe.Size<char>());

    if (strcmp(mem.get<char>(), "And yourself!"))
        fail("expected message was not written");
    if (strcmp(unsafe.get<char>(), "And yourself!"))
        fail("expected message was not written");

    if (!DeallocShmem(mem))
        fail("DeallocShmem");
    if (!DeallocShmem(unsafe))
        fail("DeallocShmem");

    Close();

    return IPC_OK();
}

//-----------------------------------------------------------------------------
// Child

mozilla::ipc::IPCResult
TestShmemChild::RecvGive(Shmem&& mem, Shmem&& unsafe, const size_t& expectedSize)
{
    if (mem.Size<char>() != expectedSize)
        fail("expected shmem size %lu, but it has size %lu",
             expectedSize, mem.Size<char>());
    if (unsafe.Size<char>() != expectedSize)
        fail("expected shmem size %lu, but it has size %lu",
             expectedSize, unsafe.Size<char>());

    if (strcmp(mem.get<char>(), "Hello!"))
        fail("expected message was not written");
    if (strcmp(unsafe.get<char>(), "Hello!"))
        fail("expected message was not written");

    char* unsafeptr = unsafe.get<char>();

    memcpy(mem.get<char>(), "And yourself!", sizeof("And yourself!"));
    memcpy(unsafeptr, "And yourself!", sizeof("And yourself!"));

    Shmem unsafecopy = unsafe;
    if (!SendTake(mem, unsafe, expectedSize))
        fail("can't send Take()");

    // these checks also shouldn't fail in the child
    char uc1 = *unsafeptr;  (void)uc1;
    char uc2 = *unsafecopy.get<char>(); (void)uc2;

    return IPC_OK();
}


} // namespace _ipdltest
} // namespace mozilla
