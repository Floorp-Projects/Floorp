#include <qapplication.h>
#include "mainwindow.h"
#include "qgeckoembed.h"
#include "nsXPCOMGlue.h"
#ifdef MOZ_JPROF
#include "jprof/jprof.h"
#endif

#include <qdir.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    static const GREVersionRange greVersion = {
        "1.9a", PR_TRUE,
        "2", PR_TRUE
    };

#ifdef MOZ_JPROF
    setupProfilingStuff();
#endif

    char xpcomPath[PATH_MAX];

    nsresult rv = GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0,
                  xpcomPath, sizeof(xpcomPath));
    if (NS_FAILED(rv)) {
        fprintf(stderr, "Couldn't find a compatible GRE.\n");
        return 1;
    }

    rv = XPCOMGlueStartup(xpcomPath);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "Couldn't start XPCOM.\n");
        return 1;
    }
    char *lastSlash = strrchr(xpcomPath, '/');
    if (lastSlash)
       *lastSlash = '\0';

    QGeckoEmbed::initialize(QDir::homePath().toUtf8(),
                            ".TestQGeckoEmbed", xpcomPath);

    MyMainWindow *mainWindow = new MyMainWindow();
    //app.setMainWidget(mainWindow);

    mainWindow->resize(400, 600);
    mainWindow->show();

    QString url;
    if (argc > 1)
        url = argv[1];
    else
        url = "http://www.kde.org";

    mainWindow->qecko->loadURL(url);

    return app.exec();
}
