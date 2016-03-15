//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// angle_deqp_gtest:
//   dEQP and GoogleTest integration logic. Calls through to the random
//   order executor.

#include <fstream>
#include <stdint.h>

#include <gtest/gtest.h>

#include "angle_deqp_libtester.h"
#include "common/angleutils.h"
#include "common/debug.h"
#include "common/Optional.h"
#include "common/string_utils.h"
#include "gpu_test_expectations_parser.h"
#include "system_utils.h"

namespace
{

const char *g_CaseListRelativePath = "/../../third_party/deqp/src/android/cts/master/";

const char *g_TestExpectationsSearchPaths[] = {
    "/../../src/tests/deqp_support/", "/../../third_party/angle/src/tests/deqp_support/",
    "/deqp_support/",
};

const char *g_CaseListFiles[] = {
    "gles2-master.txt", "gles3-master.txt", "egl-master.txt",
};

const char *g_TestExpectationsFiles[] =
{
    "deqp_gles2_test_expectations.txt",
    "deqp_gles3_test_expectations.txt",
    "deqp_egl_test_expectations.txt",
};

// During the CaseList initialization we cannot use the GTEST FAIL macro to quit the program because
// the initialization is called outside of tests the first time.
void Die()
{
    exit(EXIT_FAILURE);
}

Optional<std::string> FindTestExpectationsPath(const std::string &exeDir,
                                                      size_t testModuleIndex)
{
    for (const char *testPath : g_TestExpectationsSearchPaths)
    {
        std::stringstream testExpectationsPathStr;
        testExpectationsPathStr << exeDir << testPath << g_TestExpectationsFiles[testModuleIndex];

        std::string path = testExpectationsPathStr.str();
        std::ifstream inFile(path.c_str());
        if (!inFile.fail())
        {
            inFile.close();
            return Optional<std::string>(path);
        }
    }

    return Optional<std::string>::Invalid();
}

class dEQPCaseList
{
  public:
    dEQPCaseList(size_t testModuleIndex);

    struct CaseInfo
    {
        CaseInfo(const std::string &dEQPName,
                 const std::string &gTestName,
                 int expectation)
            : mDEQPName(dEQPName),
              mGTestName(gTestName),
              mExpectation(expectation)
        {
        }

        std::string mDEQPName;
        std::string mGTestName;
        int mExpectation;
    };

    void initialize();

    const CaseInfo &getCaseInfo(size_t caseIndex) const
    {
        ASSERT(mInitialized);
        ASSERT(caseIndex < mCaseInfoList.size());
        return mCaseInfoList[caseIndex];
    }

    size_t numCases() const
    {
        ASSERT(mInitialized);
        return mCaseInfoList.size();
    }

  private:
    std::vector<CaseInfo> mCaseInfoList;
    gpu::GPUTestExpectationsParser mTestExpectationsParser;
    gpu::GPUTestBotConfig mTestConfig;
    size_t mTestModuleIndex;
    bool mInitialized;
};

dEQPCaseList::dEQPCaseList(size_t testModuleIndex)
    : mTestModuleIndex(testModuleIndex),
      mInitialized(false)
{
}

void dEQPCaseList::initialize()
{
    if (mInitialized)
    {
        return;
    }

    mInitialized = true;

    std::string exeDir = angle::GetExecutableDirectory();

    std::stringstream caseListPathStr;
    caseListPathStr << exeDir << g_CaseListRelativePath << g_CaseListFiles[mTestModuleIndex];
    std::string caseListPath = caseListPathStr.str();

    Optional<std::string> testExpectationsPath = FindTestExpectationsPath(exeDir, mTestModuleIndex);
    if (!testExpectationsPath.valid())
    {
        std::cerr << "Failed to find test expectations file." << std::endl;
        Die();
    }

    if (!mTestExpectationsParser.LoadTestExpectationsFromFile(testExpectationsPath.value()))
    {
        std::stringstream errorMsgStream;
        for (const auto &message : mTestExpectationsParser.GetErrorMessages())
        {
            errorMsgStream << std::endl << " " << message;
        }

        std::cerr << "Failed to load test expectations." << errorMsgStream.str() << std::endl;
        Die();
    }

    if (!mTestConfig.LoadCurrentConfig(nullptr))
    {
        std::cerr << "Failed to load test configuration." << std::endl;
        Die();
    }

    std::ifstream caseListStream(caseListPath);
    if (caseListStream.fail())
    {
        std::cerr << "Failed to load the case list." << std::endl;
        Die();
    }

    while (!caseListStream.eof())
    {
        std::string inString;
        std::getline(caseListStream, inString);

        std::string dEQPName = angle::TrimString(inString, angle::kWhitespaceASCII);
        if (dEQPName.empty())
            continue;
        std::string gTestName = dEQPName.substr(dEQPName.find('.') + 1);
        if (gTestName.empty())
            continue;
        std::replace(gTestName.begin(), gTestName.end(), '.', '_');

        // Occurs in some luminance tests
        gTestName.erase(std::remove(gTestName.begin(), gTestName.end(), '-'), gTestName.end());

        int expectation = mTestExpectationsParser.GetTestExpectation(dEQPName, mTestConfig);
        if (expectation != gpu::GPUTestExpectationsParser::kGpuTestSkip)
        {
            mCaseInfoList.push_back(CaseInfo(dEQPName, gTestName, expectation));
        }
    }
}

template <size_t TestModuleIndex>
class dEQPTest : public testing::TestWithParam<size_t>
{
  public:
    static testing::internal::ParamGenerator<size_t> GetTestingRange()
    {
        return testing::Range<size_t>(0, GetCaseList().numCases());
    }

    static std::string GetCaseGTestName(size_t caseIndex)
    {
        const auto &caseInfo = GetCaseList().getCaseInfo(caseIndex);
        return caseInfo.mGTestName;
    }

    static const dEQPCaseList &GetCaseList()
    {
        static dEQPCaseList sCaseList(TestModuleIndex);
        sCaseList.initialize();
        return sCaseList;
    }

  protected:
    void runTest()
    {
        const auto &caseInfo = GetCaseList().getCaseInfo(GetParam());
        std::cout << caseInfo.mDEQPName << std::endl;

        bool result = deqp_libtester_run(caseInfo.mDEQPName.c_str());

        if (caseInfo.mExpectation == gpu::GPUTestExpectationsParser::kGpuTestPass)
        {
            EXPECT_TRUE(result);
        }
        else if (result)
        {
            std::cout << "Test expected to fail but passed!" << std::endl;
        }
    }
};

class dEQP_GLES2 : public dEQPTest<0>
{
};

class dEQP_GLES3 : public dEQPTest<1>
{
};

class dEQP_EGL : public dEQPTest<2>
{
};

// TODO(jmadill): add different platform configs, or ability to choose platform
#define ANGLE_INSTANTIATE_DEQP_TEST_CASE(DEQP_TEST)                             \
    TEST_P(DEQP_TEST, Default) { runTest(); }                                   \
                                                                                \
    INSTANTIATE_TEST_CASE_P(, DEQP_TEST, DEQP_TEST::GetTestingRange(),          \
                            [](const testing::TestParamInfo<size_t> &info)      \
                            {                                                   \
                                return DEQP_TEST::GetCaseGTestName(info.param); \
                            })

#ifdef ANGLE_DEQP_GLES2_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(dEQP_GLES2);
#endif

#ifdef ANGLE_DEQP_GLES3_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(dEQP_GLES3);
#endif

#ifdef ANGLE_DEQP_EGL_TESTS
ANGLE_INSTANTIATE_DEQP_TEST_CASE(dEQP_EGL);
#endif

} // anonymous namespace
