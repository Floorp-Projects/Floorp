/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

class ServiceWorkerRegistrarTest : public ServiceWorkerRegistrar
{
public:
  ServiceWorkerRegistrarTest()
  {
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(mProfileDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  nsresult TestReadData() { return ReadData(); }
  nsresult TestWriteData() { return WriteData(); }
  void TestDeleteData() { DeleteData(); }

  nsTArray<ServiceWorkerRegistrationData>& TestGetData() { return mData; }
};

already_AddRefed<nsIFile>
GetFile()
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  file->Append(NS_LITERAL_STRING(SERVICEWORKERREGISTRAR_FILE));
  return file.forget();
}

bool
CreateFile(const nsACString& aData)
{
  nsCOMPtr<nsIFile> file = GetFile();

  nsCOMPtr<nsIOutputStream> stream;
  nsresult rv = NS_NewLocalFileOutputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t count;
  rv = stream->Write(aData.Data(), aData.Length(), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (count != aData.Length()) {
    return false;
  }

  return true;
}

TEST(ServiceWorkerRegistrar, TestNoFile)
{
  nsCOMPtr<nsIFile> file = GetFile();
  ASSERT_TRUE(file) << "GetFile must return a nsIFIle";

  bool exists;
  nsresult rv = file->Exists(&exists);
  ASSERT_EQ(NS_OK, rv) << "nsIFile::Exists cannot fail";

  if (exists) {
    rv = file->Remove(false);
    ASSERT_EQ(NS_OK, rv) << "nsIFile::Remove cannot fail";
  }

  nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestEmptyFile)
{
  ASSERT_TRUE(CreateFile(EmptyCString())) << "CreateFile should not fail";

  nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_NE(NS_OK, rv) << "ReadData() should fail if the file is empty";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestRightVersionFile)
{
  ASSERT_TRUE(CreateFile(nsAutoCString(SERVICEWORKERREGISTRAR_VERSION "\n"))) << "CreateFile should not fail";

  nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail when the version is correct";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestWrongVersionFile)
{
  ASSERT_TRUE(CreateFile(nsAutoCString(SERVICEWORKERREGISTRAR_VERSION "bla\n"))) << "CreateFile should not fail";

  nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_NE(NS_OK, rv) << "ReadData() should fail when the version is not correct";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestReadData)
{
  nsAutoCString buffer(SERVICEWORKERREGISTRAR_VERSION "\n");

  buffer.Append(SERVICEWORKERREGISTRAR_SYSTEM_PRINCIPAL "\n");
  buffer.Append("scope 0\nscriptSpec 0\ncurrentWorkerURL 0\nactiveCache 0\nwaitingCache 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append(SERVICEWORKERREGISTRAR_CONTENT_PRINCIPAL "\n");
  buffer.Append("123\n" SERVICEWORKERREGISTRAR_TRUE "\n");
  buffer.Append("spec 1\nscope 1\nscriptSpec 1\ncurrentWorkerURL 1\nactiveCache 1\nwaitingCache 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append(SERVICEWORKERREGISTRAR_CONTENT_PRINCIPAL "\n");
  buffer.Append("0\n" SERVICEWORKERREGISTRAR_FALSE "\n");
  buffer.Append("spec 2\nscope 2\nscriptSpec 2\ncurrentWorkerURL 2\nactiveCache 2\nwaitingCache 2\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer)) << "CreateFile should not fail";

  nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)3, data.Length()) << "4 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo) << "First principal must be system";

  ASSERT_STREQ("scope 0", data[0].scope().get());
  ASSERT_STREQ("scriptSpec 0", data[0].scriptSpec().get());
  ASSERT_STREQ("currentWorkerURL 0", data[0].currentWorkerURL().get());
  ASSERT_STREQ("activeCache 0", NS_ConvertUTF16toUTF8(data[0].activeCacheName()).get());
  ASSERT_STREQ("waitingCache 0", NS_ConvertUTF16toUTF8(data[0].waitingCacheName()).get());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 = data[1].principal();

  ASSERT_EQ((uint32_t)123, cInfo1.appId());
  ASSERT_EQ((uint32_t)true, cInfo1.isInBrowserElement());
  ASSERT_STREQ("spec 1", cInfo1.spec().get());
  ASSERT_STREQ("scope 1", data[1].scope().get());
  ASSERT_STREQ("scriptSpec 1", data[1].scriptSpec().get());
  ASSERT_STREQ("currentWorkerURL 1", data[1].currentWorkerURL().get());
  ASSERT_STREQ("activeCache 1", NS_ConvertUTF16toUTF8(data[1].activeCacheName()).get());
  ASSERT_STREQ("waitingCache 1", NS_ConvertUTF16toUTF8(data[1].waitingCacheName()).get());

  const mozilla::ipc::PrincipalInfo& info2 = data[2].principal();
  ASSERT_EQ(info2.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo2 = data[2].principal();

  ASSERT_EQ((uint32_t)0, cInfo2.appId());
  ASSERT_EQ((uint32_t)false, cInfo2.isInBrowserElement());
  ASSERT_STREQ("spec 2", cInfo2.spec().get());
  ASSERT_STREQ("scope 2", data[2].scope().get());
  ASSERT_STREQ("scriptSpec 2", data[2].scriptSpec().get());
  ASSERT_STREQ("currentWorkerURL 2", data[2].currentWorkerURL().get());
  ASSERT_STREQ("activeCache 2", NS_ConvertUTF16toUTF8(data[2].activeCacheName()).get());
  ASSERT_STREQ("waitingCache 2", NS_ConvertUTF16toUTF8(data[2].waitingCacheName()).get());
}

TEST(ServiceWorkerRegistrar, TestDeleteData)
{
  ASSERT_TRUE(CreateFile(nsAutoCString("Foobar"))) << "CreateFile should not fail";

  nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  swr->TestDeleteData();

  nsCOMPtr<nsIFile> file = GetFile();

  bool exists;
  nsresult rv = file->Exists(&exists);
  ASSERT_EQ(NS_OK, rv) << "nsIFile::Exists cannot fail";

  ASSERT_FALSE(exists) << "The file should not exist after a DeleteData().";
}

TEST(ServiceWorkerRegistrar, TestWriteData)
{
  {
    nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

    nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();

    for (int i = 0; i < 10; ++i) {
      ServiceWorkerRegistrationData* d = data.AppendElement();

      if ((i % 2) == 0) {
        d->principal() = mozilla::ipc::SystemPrincipalInfo();
      } else if ((i % 2) == 1) {
        nsAutoCString spec;
        spec.AppendPrintf("spec write %d", i);
        d->principal() = mozilla::ipc::ContentPrincipalInfo(i, i % 2, spec);
      }

      d->scope().AppendPrintf("scope write %d", i);
      d->scriptSpec().AppendPrintf("scriptSpec write %d", i);
      d->currentWorkerURL().AppendPrintf("currentWorkerURL write %d", i);
      d->activeCacheName().AppendPrintf("activeCacheName write %d", i);
      d->waitingCacheName().AppendPrintf("waitingCacheName write %d", i);
    }

    nsresult rv = swr->TestWriteData();
    ASSERT_EQ(NS_OK, rv) << "WriteData() should not fail";
  }

  nsRefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)10, data.Length()) << "10 entries should be found";

  for (int i = 0; i < 10; ++i) {
    nsAutoCString test;

    if ((i % 2) == 0) {
      ASSERT_EQ(data[i].principal().type(), mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo);
    } else if ((i % 2) == 1) {
      ASSERT_EQ(data[i].principal().type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
      const mozilla::ipc::ContentPrincipalInfo& cInfo = data[i].principal();

      ASSERT_EQ((uint32_t)i, cInfo.appId());
      ASSERT_EQ((uint32_t)(i %2), cInfo.isInBrowserElement());

      test.AppendPrintf("spec write %d", i);
      ASSERT_STREQ(test.get(), cInfo.spec().get());
    }

    test.Truncate();
    test.AppendPrintf("scope write %d", i);
    ASSERT_STREQ(test.get(), data[i].scope().get());

    test.Truncate();
    test.AppendPrintf("scriptSpec write %d", i);
    ASSERT_STREQ(test.get(), data[i].scriptSpec().get());

    test.Truncate();
    test.AppendPrintf("currentWorkerURL write %d", i);
    ASSERT_STREQ(test.get(), data[i].currentWorkerURL().get());

    test.Truncate();
    test.AppendPrintf("activeCacheName write %d", i);
    ASSERT_STREQ(test.get(), NS_ConvertUTF16toUTF8(data[i].activeCacheName()).get());

    test.Truncate();
    test.AppendPrintf("waitingCacheName write %d", i);
    ASSERT_STREQ(test.get(), NS_ConvertUTF16toUTF8(data[i].waitingCacheName()).get());
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  return rv;
}
