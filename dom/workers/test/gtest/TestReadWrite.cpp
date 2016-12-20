/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"

#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

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

  void TestRegisterServiceWorker(const ServiceWorkerRegistrationData& aData)
  {
    RegisterServiceWorkerInternal(aData);
  }

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

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestEmptyFile)
{
  ASSERT_TRUE(CreateFile(EmptyCString())) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_NE(NS_OK, rv) << "ReadData() should fail if the file is empty";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestRightVersionFile)
{
  ASSERT_TRUE(CreateFile(NS_LITERAL_CSTRING(SERVICEWORKERREGISTRAR_VERSION "\n"))) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail when the version is correct";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestWrongVersionFile)
{
  ASSERT_TRUE(CreateFile(NS_LITERAL_CSTRING(SERVICEWORKERREGISTRAR_VERSION "bla\n"))) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_NE(NS_OK, rv) << "ReadData() should fail when the version is not correct";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)0, data.Length()) << "No data should be found in an empty file";
}

TEST(ServiceWorkerRegistrar, TestReadData)
{
  nsAutoCString buffer(SERVICEWORKERREGISTRAR_VERSION "\n");

  buffer.Append("^appId=123&inBrowser=1\n");
  buffer.Append("scope 0\ncurrentWorkerURL 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TRUE "\n");
  buffer.Append("cacheName 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append("\n");
  buffer.Append("scope 1\ncurrentWorkerURL 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_FALSE "\n");
  buffer.Append("cacheName 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer)) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 = data[0].principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("^appId=123&inBrowser=1", suffix0.get());
  ASSERT_STREQ("scope 0", cInfo0.spec().get());
  ASSERT_STREQ("scope 0", data[0].scope().get());
  ASSERT_STREQ("currentWorkerURL 0", data[0].currentWorkerURL().get());
  ASSERT_TRUE(data[0].currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0", NS_ConvertUTF16toUTF8(data[0].cacheName()).get());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 = data[1].principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("scope 1", cInfo1.spec().get());
  ASSERT_STREQ("scope 1", data[1].scope().get());
  ASSERT_STREQ("currentWorkerURL 1", data[1].currentWorkerURL().get());
  ASSERT_FALSE(data[1].currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1", NS_ConvertUTF16toUTF8(data[1].cacheName()).get());
}

TEST(ServiceWorkerRegistrar, TestDeleteData)
{
  ASSERT_TRUE(CreateFile(NS_LITERAL_CSTRING("Foobar"))) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

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
    RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

    for (int i = 0; i < 10; ++i) {
      ServiceWorkerRegistrationData reg;

      reg.scope() = nsPrintfCString("scope write %d", i);
      reg.currentWorkerURL() = nsPrintfCString("currentWorkerURL write %d", i);
      reg.cacheName() =
        NS_ConvertUTF8toUTF16(nsPrintfCString("cacheName write %d", i));

      nsAutoCString spec;
      spec.AppendPrintf("spec write %d", i);
      reg.principal() =
        mozilla::ipc::ContentPrincipalInfo(mozilla::PrincipalOriginAttributes(i, i % 2), spec);

      swr->TestRegisterServiceWorker(reg);
    }

    nsresult rv = swr->TestWriteData();
    ASSERT_EQ(NS_OK, rv) << "WriteData() should not fail";
  }

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)10, data.Length()) << "10 entries should be found";

  for (int i = 0; i < 10; ++i) {
    nsAutoCString test;

    ASSERT_EQ(data[i].principal().type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
    const mozilla::ipc::ContentPrincipalInfo& cInfo = data[i].principal();

    mozilla::PrincipalOriginAttributes attrs(i, i % 2);
    nsAutoCString suffix, expectSuffix;
    attrs.CreateSuffix(expectSuffix);
    cInfo.attrs().CreateSuffix(suffix);

    ASSERT_STREQ(expectSuffix.get(), suffix.get());

    test.AppendPrintf("scope write %d", i);
    ASSERT_STREQ(test.get(), cInfo.spec().get());

    test.Truncate();
    test.AppendPrintf("scope write %d", i);
    ASSERT_STREQ(test.get(), data[i].scope().get());

    test.Truncate();
    test.AppendPrintf("currentWorkerURL write %d", i);
    ASSERT_STREQ(test.get(), data[i].currentWorkerURL().get());

    test.Truncate();
    test.AppendPrintf("cacheName write %d", i);
    ASSERT_STREQ(test.get(), NS_ConvertUTF16toUTF8(data[i].cacheName()).get());
  }
}

TEST(ServiceWorkerRegistrar, TestVersion2Migration)
{
  nsAutoCString buffer("2" "\n");

  buffer.Append("^appId=123&inBrowser=1\n");
  buffer.Append("spec 0\nscope 0\nscriptSpec 0\ncurrentWorkerURL 0\nactiveCache 0\nwaitingCache 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append("\n");
  buffer.Append("spec 1\nscope 1\nscriptSpec 1\ncurrentWorkerURL 1\nactiveCache 1\nwaitingCache 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer)) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 = data[0].principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("^appId=123&inBrowser=1", suffix0.get());
  ASSERT_STREQ("scope 0", cInfo0.spec().get());
  ASSERT_STREQ("scope 0", data[0].scope().get());
  ASSERT_STREQ("currentWorkerURL 0", data[0].currentWorkerURL().get());
  ASSERT_STREQ("activeCache 0", NS_ConvertUTF16toUTF8(data[0].cacheName()).get());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 = data[1].principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("scope 1", cInfo1.spec().get());
  ASSERT_STREQ("scope 1", data[1].scope().get());
  ASSERT_STREQ("currentWorkerURL 1", data[1].currentWorkerURL().get());
  ASSERT_STREQ("activeCache 1", NS_ConvertUTF16toUTF8(data[1].cacheName()).get());
}

TEST(ServiceWorkerRegistrar, TestVersion3Migration)
{
  nsAutoCString buffer("3" "\n");

  buffer.Append("^appId=123&inBrowser=1\n");
  buffer.Append("spec 0\nscope 0\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append("\n");
  buffer.Append("spec 1\nscope 1\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer)) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 = data[0].principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("^appId=123&inBrowser=1", suffix0.get());
  ASSERT_STREQ("scope 0", cInfo0.spec().get());
  ASSERT_STREQ("scope 0", data[0].scope().get());
  ASSERT_STREQ("currentWorkerURL 0", data[0].currentWorkerURL().get());
  ASSERT_STREQ("cacheName 0", NS_ConvertUTF16toUTF8(data[0].cacheName()).get());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 = data[1].principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("scope 1", cInfo1.spec().get());
  ASSERT_STREQ("scope 1", data[1].scope().get());
  ASSERT_STREQ("currentWorkerURL 1", data[1].currentWorkerURL().get());
  ASSERT_STREQ("cacheName 1", NS_ConvertUTF16toUTF8(data[1].cacheName()).get());
}

TEST(ServiceWorkerRegistrar, TestVersion4Migration)
{
  nsAutoCString buffer("4" "\n");

  buffer.Append("^appId=123&inBrowser=1\n");
  buffer.Append("scope 0\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append("\n");
  buffer.Append("scope 1\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer)) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 = data[0].principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("^appId=123&inBrowser=1", suffix0.get());
  ASSERT_STREQ("scope 0", cInfo0.spec().get());
  ASSERT_STREQ("scope 0", data[0].scope().get());
  ASSERT_STREQ("currentWorkerURL 0", data[0].currentWorkerURL().get());
  // default is true
  ASSERT_EQ(true, data[1].currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 0", NS_ConvertUTF16toUTF8(data[0].cacheName()).get());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 = data[1].principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("scope 1", cInfo1.spec().get());
  ASSERT_STREQ("scope 1", data[1].scope().get());
  ASSERT_STREQ("currentWorkerURL 1", data[1].currentWorkerURL().get());
  // default is true
  ASSERT_EQ(true, data[1].currentWorkerHandlesFetch());
  ASSERT_STREQ("cacheName 1", NS_ConvertUTF16toUTF8(data[1].cacheName()).get());
}

TEST(ServiceWorkerRegistrar, TestDedupeRead)
{
  nsAutoCString buffer("3" "\n");

  // unique entries
  buffer.Append("^appId=123&inBrowser=1\n");
  buffer.Append("spec 0\nscope 0\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append("\n");
  buffer.Append("spec 1\nscope 1\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  // dupe entries
  buffer.Append("^appId=123&inBrowser=1\n");
  buffer.Append("spec 1\nscope 0\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append("^appId=123&inBrowser=1\n");
  buffer.Append("spec 2\nscope 0\ncurrentWorkerURL 0\ncacheName 0\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  buffer.Append("\n");
  buffer.Append("spec 3\nscope 1\ncurrentWorkerURL 1\ncacheName 1\n");
  buffer.Append(SERVICEWORKERREGISTRAR_TERMINATOR "\n");

  ASSERT_TRUE(CreateFile(buffer)) << "CreateFile should not fail";

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)2, data.Length()) << "2 entries should be found";

  const mozilla::ipc::PrincipalInfo& info0 = data[0].principal();
  ASSERT_EQ(info0.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo0 = data[0].principal();

  nsAutoCString suffix0;
  cInfo0.attrs().CreateSuffix(suffix0);

  ASSERT_STREQ("^appId=123&inBrowser=1", suffix0.get());
  ASSERT_STREQ("scope 0", cInfo0.spec().get());
  ASSERT_STREQ("scope 0", data[0].scope().get());
  ASSERT_STREQ("currentWorkerURL 0", data[0].currentWorkerURL().get());
  ASSERT_STREQ("cacheName 0", NS_ConvertUTF16toUTF8(data[0].cacheName()).get());

  const mozilla::ipc::PrincipalInfo& info1 = data[1].principal();
  ASSERT_EQ(info1.type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo) << "First principal must be content";
  const mozilla::ipc::ContentPrincipalInfo& cInfo1 = data[1].principal();

  nsAutoCString suffix1;
  cInfo1.attrs().CreateSuffix(suffix1);

  ASSERT_STREQ("", suffix1.get());
  ASSERT_STREQ("scope 1", cInfo1.spec().get());
  ASSERT_STREQ("scope 1", data[1].scope().get());
  ASSERT_STREQ("currentWorkerURL 1", data[1].currentWorkerURL().get());
  ASSERT_STREQ("cacheName 1", NS_ConvertUTF16toUTF8(data[1].cacheName()).get());
}

TEST(ServiceWorkerRegistrar, TestDedupeWrite)
{
  {
    RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

    for (int i = 0; i < 10; ++i) {
      ServiceWorkerRegistrationData reg;

      reg.scope() = NS_LITERAL_CSTRING("scope write dedupe");
      reg.currentWorkerURL() = nsPrintfCString("currentWorkerURL write %d", i);
      reg.cacheName() =
        NS_ConvertUTF8toUTF16(nsPrintfCString("cacheName write %d", i));

      nsAutoCString spec;
      spec.AppendPrintf("spec write dedupe/%d", i);
      reg.principal() =
        mozilla::ipc::ContentPrincipalInfo(mozilla::PrincipalOriginAttributes(0, false), spec);

      swr->TestRegisterServiceWorker(reg);
    }

    nsresult rv = swr->TestWriteData();
    ASSERT_EQ(NS_OK, rv) << "WriteData() should not fail";
  }

  RefPtr<ServiceWorkerRegistrarTest> swr = new ServiceWorkerRegistrarTest;

  nsresult rv = swr->TestReadData();
  ASSERT_EQ(NS_OK, rv) << "ReadData() should not fail";

  // Duplicate entries should be removed.
  const nsTArray<ServiceWorkerRegistrationData>& data = swr->TestGetData();
  ASSERT_EQ((uint32_t)1, data.Length()) << "1 entry should be found";

  ASSERT_EQ(data[0].principal().type(), mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
  const mozilla::ipc::ContentPrincipalInfo& cInfo = data[0].principal();

  mozilla::PrincipalOriginAttributes attrs(0, false);
  nsAutoCString suffix, expectSuffix;
  attrs.CreateSuffix(expectSuffix);
  cInfo.attrs().CreateSuffix(suffix);

  // Last entry passed to RegisterServiceWorkerInternal() should overwrite
  // previous values.  So expect "9" in values here.
  ASSERT_STREQ(expectSuffix.get(), suffix.get());
  ASSERT_STREQ("scope write dedupe", cInfo.spec().get());
  ASSERT_STREQ("scope write dedupe", data[0].scope().get());
  ASSERT_STREQ("currentWorkerURL write 9", data[0].currentWorkerURL().get());
  ASSERT_STREQ("cacheName write 9",
               NS_ConvertUTF16toUTF8(data[0].cacheName()).get());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  return rv;
}
