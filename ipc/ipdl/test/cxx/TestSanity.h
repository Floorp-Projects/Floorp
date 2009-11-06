#ifndef mozilla__ipdltest_TestSanity_h
#define mozilla__ipdltest_TestSanity_h 1


#include "mozilla/_ipdltest/PTestSanityParent.h"
#include "mozilla/_ipdltest/PTestSanityChild.h"

namespace mozilla {
namespace _ipdltest {


class TestSanityParent :
    public PTestSanityParent
{
public:
    TestSanityParent();
    virtual ~TestSanityParent();

    void Main();

protected:    
    virtual bool RecvPong(const int& one, const float& zeroPtTwoFive);
    virtual bool RecvUNREACHED();
};


class TestSanityChild :
    public PTestSanityChild
{
public:
    TestSanityChild();
    virtual ~TestSanityChild();

protected:
    virtual bool RecvPing(const int& zero, const float& zeroPtFive);
    virtual bool RecvUNREACHED();
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_TestSanity_h
