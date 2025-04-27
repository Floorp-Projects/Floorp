import { Button } from "@/components/ui/button.tsx"
import { Card, CardContent, CardFooter, CardHeader, CardTitle } from "@/components/ui/card.tsx"
import { Separator } from "@/components/ui/separator.tsx"
import { SiGithub } from '@icons-pack/react-simple-icons';
import { Scale } from "lucide-react"

export default function Page() {
    return (
        <div className="flex p-4">
            <Card>
                <CardHeader>
                    <CardTitle>About Noraneko Browser</CardTitle>
                </CardHeader>
                <CardContent>
                    <p>Noraneko is a browser as testhead of Floorp 12.</p>
                    <p>Made by Noraneko Community with ❤</p>
                </CardContent>
                <CardFooter>
                    <div>
                        <Separator />
                        <div className="pt-4">Binaries: Mozilla Public License 2.0 (MPL)</div>
                        <div className="pt-4">
                            <Button asChild>
                                <a href="about:license"><Scale />License Notice: Know your rights</a>
                            </Button>
                        </div>
                        <div className="pt-2">
                            <Button asChild>
                                <a href="https://github.com/nyanrus/noraneko"><SiGithub />GitHub Repository: nyanrus/noraneko</a>
                            </Button>
                        </div>
                    </div>
                </CardFooter>
            </Card>
        </div>
    );
}
