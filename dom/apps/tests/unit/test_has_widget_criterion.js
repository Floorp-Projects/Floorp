/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource:///modules/AppsUtils.jsm");

add_test(function test_has_widget_criterion() {

  // follow the logic |_saveWidgetsFullPath|
  let baseUri = Services.io.newURI('http://example.com', null, null);
  let resolve = (aPage)=>{
    let filepath = AppsUtils.getFilePath(aPage);

    return baseUri.resolve(filepath);
  };

  let widgetPages = ['/widget.html',
                     '/foo/bar.html'];
  let resolvedWidgetPages = widgetPages.map(resolve);

  let app = new mozIApplication({widgetPages:resolvedWidgetPages});

  let widgetPageCheck = aPage => app.hasWidgetPage(baseUri.resolve(aPage));

  Assert.ok(widgetPageCheck('/widget.html'), 'should pass for identical path');
  Assert.ok(widgetPageCheck('/foo/bar.html'), 'should pass for identical path');

  Assert.ok(!widgetPageCheck('/wrong.html'), 'should not pass for wrong path');
  Assert.ok(!widgetPageCheck('/WIDGET.html'), 'should be case _sensitive_ for path');
  Assert.ok(!widgetPageCheck('/widget.HTML'), 'should be case _sensitive_ for file extension');

  Assert.ok(widgetPageCheck('/widget.html?aQuery'), 'should be query insensitive');
  Assert.ok(widgetPageCheck('/widget.html#aHash'), 'should be hash insensitive');
  Assert.ok(widgetPageCheck('/widget.html?aQuery=aquery#aHash'),
                            'should be hash/query insensitive');

  Assert.ok(widgetPageCheck('HTTP://example.com/widget.html'),
                            'should be case insensitive for protocol');
  Assert.ok(widgetPageCheck('http://EXAMPLE.COM/widget.html'),
                            'should be case insensitive for domain');
  Assert.ok(widgetPageCheck('http://example.com:80/widget.html'),
                            'should pass for default port');

  Assert.ok(widgetPageCheck('HTTP://EXAMPLE.COM:80/widget.html?QueryA=queryA&QueryB=queryB#aHash'),
                            'should pass for a really mess one');

  Assert.ok(!widgetPageCheck('foo://example.com/widget.html'),
                            'should not pass for wrong protocol');
  Assert.ok(!widgetPageCheck('https://example.com/widget.html'),
                            'should not pass for wrong protocol');
  Assert.ok(!widgetPageCheck('/wrong/widget.html'),
                            'should not pass for additional path');
  Assert.ok(!widgetPageCheck('/bar.html'),
                            'should not pass for reduced path');
  Assert.ok(!widgetPageCheck('http://username:password@example.com/widget.html'),
                            'should not pass for userinfo');
  Assert.ok(!widgetPageCheck('http://example.com:8080/widget.html'),
                             'should not pass non-default port');
  run_next_test();
});

function run_test() {
  run_next_test();
}
