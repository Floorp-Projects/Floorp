/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Unit test for nsMetricsConfig

#include "TestCommon.h"
#include "nsMetricsConfig.h"
#include "nsMetricsService.h"
#include "nsXPCOM.h"
#include "nsILocalFile.h"

#include <stdio.h>

// This singleton must exist in any code that links against libmetrics_s.
// TODO: find a way to declare this in src/ while still allowing it to be
// visible to nsMetricsModule.
NS_DECL_CLASSINFO(nsMetricsService)

static int gTotalTests = 0;
static int gPassedTests = 0;

void TestLoad(const char *testdata_path)
{
  ++gTotalTests;

  nsMetricsConfig config;
  ASSERT_TRUE(config.Init());

  nsCOMPtr<nsILocalFile> dataFile;
  NS_NewNativeLocalFile(nsDependentCString(testdata_path),
                        PR_TRUE, getter_AddRefs(dataFile));
  ASSERT_TRUE(dataFile);

  ASSERT_SUCCESS(dataFile->AppendNative(
                     NS_LITERAL_CSTRING("test_config.xml")));
  ASSERT_SUCCESS(config.Load(dataFile));

  ASSERT_TRUE(config.IsEventEnabled(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                                    NS_LITERAL_STRING("foo")));
  ASSERT_TRUE(config.IsEventEnabled(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                                    NS_LITERAL_STRING("bar")));
  ASSERT_FALSE(config.IsEventEnabled(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                                     NS_LITERAL_STRING("baz")));

  ASSERT_TRUE(config.EventLimit() == 200);
  ASSERT_TRUE(config.UploadInterval() == 1000);
  ASSERT_TRUE(config.HasConfig());
  ++gPassedTests;
}

// Returns true if the contents of |file| match |contents|.
static PRBool CheckFileContents(nsILocalFile *file, const char *contents)
{
  nsCString nativePath;
  file->GetNativePath(nativePath);

  // Now read in the file contents and compare to the expected output
  PRFileInfo info;
  ASSERT_TRUE_RET(PR_GetFileInfo(nativePath.get(), &info) == PR_SUCCESS,
                  PR_FALSE);

  char *buf = new char[info.size + 1];
  ASSERT_TRUE_RET(buf, PR_FALSE);

  PRFileDesc *fd = PR_Open(nativePath.get(), PR_RDONLY, 0);
  ASSERT_TRUE_RET(fd, PR_FALSE);

  ASSERT_TRUE_RET(PR_Read(fd, buf, info.size) == info.size, PR_FALSE);
  PR_Close(fd);
  buf[info.size] = '\0';

  // Leave the file in place if the test failed
  ASSERT_TRUE_RET(!strcmp(buf, contents), PR_FALSE);
  PR_Delete(nativePath.get());
  delete[] buf;
  return PR_TRUE;
}

void TestSave(const char *temp_data_path)
{
  ++gTotalTests;
  static const char kFilename[] = "test-save.xml";
  static const char kExpectedContents[] =
    "<response xmlns=\"http://www.mozilla.org/metrics\"><config>"
    "<collectors>"
      "<collector type=\"uielement\"/>"
    "</collectors>"
    "<limit events=\"300\"/>"
    "<upload interval=\"500\"/>"
    "</config></response>";

  nsMetricsConfig config;
  ASSERT_TRUE(config.Init());

  // The data file goes to the current directory
  config.SetEventEnabled(NS_LITERAL_STRING(NS_METRICS_NAMESPACE),
                         NS_LITERAL_STRING("uielement"), PR_TRUE);
  config.SetUploadInterval(500);
  config.SetEventLimit(300);

  nsCOMPtr<nsILocalFile> outFile;
  NS_NewNativeLocalFile(nsDependentCString(temp_data_path),
                        PR_TRUE, getter_AddRefs(outFile));
  ASSERT_TRUE(outFile);
  ASSERT_SUCCESS(outFile->AppendNative(nsDependentCString(kFilename)));

  ASSERT_SUCCESS(config.Save(outFile));
  ASSERT_TRUE(CheckFileContents(outFile, kExpectedContents));

  // Now test with no collectors
  static const char kExpectedOutputNoEvents[] =
    "<response xmlns=\"http://www.mozilla.org/metrics\"><config>"
    "<collectors/>"
    "<limit events=\"300\"/>"
    "<upload interval=\"500\"/>"
    "</config></response>";

  config.ClearEvents();
  ASSERT_SUCCESS(config.Save(outFile));
  ASSERT_TRUE(CheckFileContents(outFile, kExpectedOutputNoEvents));

  ++gPassedTests;
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s test_data_path temp_data_path\n", argv[0]);
    return 1;
  }

  TestLoad(argv[1]);
  TestSave(argv[2]);

  printf("%d/%d tests passed\n", gPassedTests, gTotalTests);
  return 0;
}
